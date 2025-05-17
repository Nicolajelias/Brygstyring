import os
import re
import shutil

# pylint: disable=undefined-variable
from SCons.Script import Import
env = Import("env")

Import("env")

# Sti til outputfilen (firmware.bin) og din Version.h-fil
firmware_path = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
version_file = "include/Version.h"  # Sørg for, at stien er korrekt, hvis filen ligger i en undermappe

# Læs Version.h og find versionsnummeret.
version = "unknown"
try:
    with open(version_file, "r") as f:
        content = f.read()
        # Forventet linje: #define SOFTWARE_VERSION "1.0.0"
        m = re.search(r'#define\s+SOFTWARE_VERSION\s+"([^"]+)"', content)
        if m:
            version = m.group(1)
except Exception as e:
    print("Fejl ved læsning af version:", e)

# Omdøb firmware.bin filen
if os.path.exists(firmware_path):
    new_firmware_name = "firmware_v" + version + ".bin"
    new_firmware_path = os.path.join(env.subst("$BUILD_DIR"), new_firmware_name)
    shutil.move(firmware_path, new_firmware_path)
    print("Firmware omdøbt til:", new_firmware_name)
else:
    print("Firmware.bin blev ikke fundet!")
