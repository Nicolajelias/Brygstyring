"""Compile and run the real temperature/process code with simulated peripherals.

Windows: uses PlatformIO's 32-bit MinGW toolchain (same unsigned-long width as ESP32).
Run after `platformio run` has installed Adafruit MAX31865 1.6.2.
"""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
compiler_dir = Path.home() / '.platformio/packages/toolchain-gccmingw32/bin'
compiler = compiler_dir / 'g++.exe'
library = root / '.pio/libdeps/esp32-s3-devkitc-1-16mb-psram/Adafruit MAX31865 library'
output = root / '.pio/test-pt100.exe'
if not compiler.exists() or not (library / 'Adafruit_MAX31865.cpp').exists():
    raise SystemExit('Requires PlatformIO toolchain-gccmingw32 and installed Adafruit MAX31865 library.')
env = dict(os.environ)
env['PATH'] = str(compiler_dir) + os.pathsep + env.get('PATH', '')
subprocess.run([
    str(compiler), '-std=c++14', '-DARDUINO=10800', '-Wall', '-Wextra',
    '-Itest/host', '-Iinclude', '-I' + str(library),
    'test/host/test_pt100.cpp', 'src/TemperatureHandler.cpp', 'src/ProcessHandler.cpp',
    'src/TemperatureCalibrationWeb.cpp',
    str(library / 'Adafruit_MAX31865.cpp'), '-static', '-o', str(output),
], cwd=root, env=env, check=True)
subprocess.run([str(output)], cwd=root, env=env, check=True)
