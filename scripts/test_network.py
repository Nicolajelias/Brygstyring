"""Compile actual EEPROM migration and Wi-Fi state machine against host peripherals."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
compiler_dir = Path.home() / '.platformio/packages/toolchain-gccmingw32/bin'
env = dict(os.environ)
env['PATH'] = str(compiler_dir) + os.pathsep + env.get('PATH', '')
output = root / '.pio/test-network.exe'
subprocess.run([str(compiler_dir / 'g++.exe'), '-std=c++14', '-Wall', '-Wextra',
               '-Itest/host', '-Iinclude', 'test/host/test_network.cpp',
               'src/WiFiHandler.cpp', 'src/EEPROMHandler.cpp', '-static', '-o', str(output)],
              cwd=root, env=env, check=True)
subprocess.run([str(output)], cwd=root, env=env, check=True)
