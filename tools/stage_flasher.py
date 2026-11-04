# After each build, pack the firmware into flasher/ for the browser installer.
#
# ESP Web Tools cannot patch flash mode and size the way esptool does during
# pio upload, so this merges the bootloader, partition table, and app into
# one image with those already filled in (DIO, 40 MHz, 4 MB — this board).
Import("env")

import datetime
import json
from pathlib import Path


def stage_flasher(source, target, env):
    build = Path(env.subst("$BUILD_DIR"))
    project = Path(env.subst("$PROJECT_DIR"))
    # One folder per board. Both chips are plain ESP32, so the installer
    # page has to ask which size rather than guessing from the chip.
    folder = "cyd35" if env["PIOENV"] == "cyd35" else "cyd28"
    out_dir = project / "flasher" / folder
    out_dir.mkdir(parents=True, exist_ok=True)

    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    boot_app0 = framework / "tools" / "partitions" / "boot_app0.bin"
    merged = out_dir / "firmware.bin"

    def q(path):
        return '"' + str(path) + '"'

    cmd = " ".join(
        [
            q(env.subst("$PYTHONEXE")),
            q(env.subst("$OBJCOPY")),
            "--chip esp32 merge_bin",
            "-o",
            q(merged),
            "--flash_mode dio --flash_freq 40m --flash_size 4MB",
            "0x1000",
            q(build / "bootloader.bin"),
            "0x8000",
            q(build / "partitions.bin"),
            "0xe000",
            q(boot_app0),
            "0x10000",
            q(build / "firmware.bin"),
        ]
    )
    env.Execute(env.VerboseAction(cmd, "Packing the web flasher image"))

    manifest = {
        "name": "CYDesk 3.5" if folder == "cyd35" else "CYDesk 2.8",
        "version": datetime.date.today().isoformat(),
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": [{"path": "firmware.bin", "offset": 0}],
            }
        ],
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Web flasher ready in flasher/{folder}/")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", stage_flasher)
