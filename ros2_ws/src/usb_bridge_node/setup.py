from setuptools import setup
import os
from glob import glob

package_name = 'usb_bridge_node'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    author='SixEyes',
    description='USB bridge node stub',
)
