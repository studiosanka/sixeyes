#include "ble_transport.h"

#ifdef SIXEYES_BLE

#include "modules/comms/host_link/host_codec.h"
#include "modules/util/logging.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace BleTransport {

namespace {

const char *SERVICE_UUID = "53495845-5945-5300-0001-000000000000";
const char *RX_UUID = "53495845-5945-5300-0002-000000000000";
const char *TX_UUID = "53495845-5945-5300-0003-000000000000";

constexpr uint16_t PREFERRED_MTU = 247;
constexpr uint16_t DEFAULT_MTU = 23;       // before the phone negotiates a larger one
constexpr size_t RX_CHUNK_MAX = 250;       // the longest write we keep (MTU 247 gives 244)
constexpr uint32_t ALIVE_MS = 1000;

HostLink *link_ = nullptr;
HostCodec::FrameParser parser_;

NimBLEServer *server_ = nullptr;
NimBLECharacteristic *tx_ = nullptr;

// Writes arrive on the NimBLE task; they are queued and handled from loop() so
// HostLink is only ever touched from one task.
struct RxChunk {
  uint16_t len;
  uint8_t data[RX_CHUNK_MAX];
};
QueueHandle_t rx_queue_ = nullptr;

// Set by the NimBLE callbacks, read by loop().
volatile uint16_t conn_handle_ = 0xFFFF; // 0xFFFF = no connection
volatile bool authorised_ = false;       // encrypted and bonded: may talk
volatile bool subscribed_ = false;       // the phone turned notifications on
volatile uint16_t mtu_ = DEFAULT_MTU;
volatile bool disconnect_event_ = false;

volatile uint32_t pairing_until_ms_ = 0; // 0 = closed
uint32_t last_rx_ms_ = 0;
bool saw_traffic_ = false;

bool windowOpenAt(uint32_t now) {
  const uint32_t until = pairing_until_ms_;
  return until != 0 && static_cast<int32_t>(now - until) < 0;
}

// While the window is closed the arm does not create new bonds, so a phone that
// pairs anyway is not bonded and is turned away in onAuthenticationComplete().
void setBondingAllowed(bool allowed) {
  NimBLEDevice::setSecurityAuth(allowed, false, true); // bonding, no MITM (Just Works), secure connections
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override {
    if (conn_handle_ != 0xFFFF) { // one phone at a time
      server->disconnect(info.getConnHandle());
      return;
    }
    conn_handle_ = info.getConnHandle();
    authorised_ = false;
    mtu_ = DEFAULT_MTU;
    // Ask for a short connection interval (15 to 30 ms, timeout 4 s) so commands
    // and state keep flowing well inside the 200 ms watchdog.
    server->updateConnParams(info.getConnHandle(), 12, 24, 0, 400);
  }

  void onDisconnect(NimBLEServer *, NimBLEConnInfo &info, int) override {
    if (info.getConnHandle() != conn_handle_) return; // a turned-away second connection
    conn_handle_ = 0xFFFF;
    authorised_ = false;
    subscribed_ = false;
    disconnect_event_ = true;
  }

  void onMTUChange(uint16_t mtu, NimBLEConnInfo &) override { mtu_ = mtu; }

  void onAuthenticationComplete(NimBLEConnInfo &info) override {
    if (info.getConnHandle() != conn_handle_) return;
    if (!info.isEncrypted() || !info.isBonded()) {
      // Paired outside the window (so it was not bonded): turn it away.
      Logging::warn("BleTransport: a phone paired outside the pairing window, disconnecting it");
      server_->disconnect(info.getConnHandle());
      return;
    }
    authorised_ = true;
  }
} server_callbacks;

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
    if (!authorised_ || !rx_queue_) return;
    const NimBLEAttValue value = characteristic->getValue();
    if (value.length() == 0 || value.length() > RX_CHUNK_MAX) return;
    RxChunk chunk;
    chunk.len = static_cast<uint16_t>(value.length());
    memcpy(chunk.data, value.data(), chunk.len);
    xQueueSend(rx_queue_, &chunk, 0); // full queue: drop; the watchdog covers a silent link
  }
} rx_callbacks;

class TxCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &info, uint16_t sub_value) override {
    if (info.getConnHandle() == conn_handle_) subscribed_ = (sub_value & 1) != 0; // bit 0 = notifications
  }
} tx_callbacks;

// Sends one encoded frame as notifications. A frame normally fits one value
// (MTU 247); with a small MTU it is split into pieces. Either way the phone
// treats the link as a byte stream: a frame ends at its own closing 0x00.
bool bleSink(const uint8_t *data, size_t len, void *) {
  if (!authorised_ || !subscribed_ || !tx_) return false;
  const size_t payload = mtu_ > 3 ? mtu_ - 3 : 20;
  const uint16_t handle = conn_handle_;
  if (handle == 0xFFFF) return false;
  size_t sent = 0;
  while (sent < len) {
    const size_t n = (len - sent) < payload ? (len - sent) : payload;
    if (!tx_->notify(data + sent, n, handle)) return false;
    sent += n;
  }
  return true;
}

void makeName(char *out, size_t cap) {
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(out, cap, "SixEyes-%02X%02X", static_cast<unsigned>((mac >> 32) & 0xFF),
           static_cast<unsigned>((mac >> 40) & 0xFF));
}

} // namespace

void init(HostLink &link) {
  link_ = &link;
  rx_queue_ = xQueueCreate(8, sizeof(RxChunk));

  char name[24];
  makeName(name, sizeof(name));
  NimBLEDevice::init(name);
  NimBLEDevice::setMTU(PREFERRED_MTU);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  setBondingAllowed(false); // opened below for the power-up window

  server_ = NimBLEDevice::createServer();
  server_->setCallbacks(&server_callbacks);
  server_->advertiseOnDisconnect(true);

  NimBLEService *service = server_->createService(SERVICE_UUID);
  NimBLECharacteristic *rx = service->createCharacteristic(
      RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
  rx->setCallbacks(&rx_callbacks);
  tx_ = service->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC);
  tx_->setCallbacks(&tx_callbacks);

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setName(name);
  adv->start();

  link.setSink(SIXEYES_CONTROL_SOURCE_BLE, bleSink, nullptr);
  openPairing(millis()); // pairing is open for the first minute after power-up
  Logging::infof("BleTransport: advertising as %s, pairing open for %lu s", name,
                 static_cast<unsigned long>(PAIRING_WINDOW_MS / 1000));
}

void openPairing(uint32_t now_ms) {
  // Keep the last MAX_BONDED_PHONES phones: make room by forgetting the oldest.
  while (NimBLEDevice::getNumBonds() >= MAX_BONDED_PHONES) {
    NimBLEDevice::deleteBond(NimBLEDevice::getBondedAddress(0));
  }
  setBondingAllowed(true);
  pairing_until_ms_ = now_ms + PAIRING_WINDOW_MS;
  if (pairing_until_ms_ == 0) pairing_until_ms_ = 1; // 0 means closed
  Logging::info("BleTransport: pairing window open");
}

bool pairingOpen() { return windowOpenAt(millis()); }

bool connected() { return conn_handle_ != 0xFFFF && authorised_; }

bool alive(uint32_t now_ms) {
  return connected() && link_ && link_->sourceConnected(SIXEYES_CONTROL_SOURCE_BLE) && saw_traffic_ &&
         static_cast<uint32_t>(now_ms - last_rx_ms_) < ALIVE_MS;
}

void update(uint32_t now_ms) {
  if (!link_) return;

  // Close the pairing window when it runs out.
  if (pairing_until_ms_ != 0 && !windowOpenAt(now_ms)) {
    pairing_until_ms_ = 0;
    setBondingAllowed(false);
    Logging::info("BleTransport: pairing window closed");
  }

  if (disconnect_event_) {
    disconnect_event_ = false;
    saw_traffic_ = false;
    parser_ = HostCodec::FrameParser(); // forget half a frame from the old link
    link_->onSourceDisconnected(SIXEYES_CONTROL_SOURCE_BLE);
    Logging::info("BleTransport: phone disconnected");
  }

  RxChunk chunk;
  HostCodec::Frame frame;
  for (int i = 0; i < 8 && xQueueReceive(rx_queue_, &chunk, 0) == pdTRUE; i++) {
    for (uint16_t k = 0; k < chunk.len; k++) {
      if (parser_.feedByte(chunk.data[k], frame)) {
        saw_traffic_ = true;
        last_rx_ms_ = now_ms;
        link_->onFrame(SIXEYES_CONTROL_SOURCE_BLE, frame, now_ms);
      }
    }
    // Nothing is added after a write. The link is a byte stream, as in PROTOCOL.md: a
    // phone at the default 23-byte MTU writes a frame in 20-byte pieces (a targets
    // frame is 30 bytes), and the frame ends at its own closing 0x00, never at the
    // end of a write. The phone always sends that 0x00.
  }

  // A small MTU splits every state frame over several notifications. Say so once
  // per connection, so a slow or choppy link is not a mystery.
  static uint16_t warned_mtu = 0;
  const uint16_t mtu = mtu_;
  if (connected() && mtu != warned_mtu) {
    warned_mtu = mtu;
    if (mtu < 70) {
      Logging::warnf("BleTransport: MTU is only %u, so each 62-byte state frame is split over several "
                     "notifications (50 Hz); the phone should negotiate a larger MTU", static_cast<unsigned>(mtu));
    }
  }
}

} // namespace BleTransport

#endif // SIXEYES_BLE
