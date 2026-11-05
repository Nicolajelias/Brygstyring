"""Run actual RGB LED task against simulated time and NeoPixel output."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
compiler_dir=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin'
env=dict(os.environ)
env['PATH']=str(compiler_dir)+os.pathsep+env.get('PATH','')
output=root/'.pio/test-led.exe'
subprocess.run([str(compiler_dir/'g++.exe'),'-std=c++14','-Wall','-Wextra',
               '-Itest/host','-Iinclude','test/host/test_led.cpp','src/StatusLED.cpp',
               '-static','-o',str(output)],cwd=root,env=env,check=True)
subprocess.run([str(output)],cwd=root,env=env,check=True)
