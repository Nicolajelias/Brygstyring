import os
import re
import shutil

# pylint: disable=undefined-variable
from SCons.Script import Import
env = Import("env")

Import("env")

# Sti til outputfilen (firmware.bin) og din Version.h-fil
build_dir = env.subst("$BUILD_DIR")
project_dir = env.subst("$PROJECT_DIR")
firmware_path = os.path.join(build_dir, "firmware.bin")
version_file = os.path.join(project_dir, "include", "Version.h")

# Læs Version.h og find versionsnummeret.
version = "unknown"
try:
    with open(version_file, "r", encoding="utf-8") as f:
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
    new_firmware_path = os.path.join(build_dir, new_firmware_name)
    shutil.move(firmware_path, new_firmware_path)

    # Placér også en kopi i projektmappen, så den er synlig fra Windows.
    export_dir = os.path.join(project_dir, "firmware")
    os.makedirs(export_dir, exist_ok=True)
    exported_path = os.path.join(export_dir, new_firmware_name)
    shutil.copy2(new_firmware_path, exported_path)

    print("Firmware omdøbt til:", new_firmware_name)
    print("Ekstra kopi placeret i:", exported_path)
else:
    print("Firmware.bin blev ikke fundet!")
