import os
import re
import shutil

from SCons.Script import Import

Import("env")


def export_firmware(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    firmware_path = str(target[0])
    version_file = os.path.join(project_dir, "include", "Version.h")

    version = "unknown"
    try:
        with open(version_file, "r", encoding="utf-8") as version_handle:
            match = re.search(
                r'#define\s+SOFTWARE_VERSION\s+"([^"]+)"',
                version_handle.read(),
            )
            if match:
                version = match.group(1)
    except OSError as error:
        print("Kunne ikke læse firmwareversion:", error)

    export_dir = os.path.join(project_dir, "firmware")
    os.makedirs(export_dir, exist_ok=True)
    exported_path = os.path.join(export_dir, f"firmware_v{version}.bin")
    shutil.copy2(firmware_path, exported_path)
    print("Firmwarekopi placeret i:", exported_path)


env.AddPostAction("$BUILD_DIR/firmware.bin", export_firmware)
