# After each build, pack the firmware into flasher/ for the browser installer.
#
# ESP Web Tools cannot patch flash mode and size the way esptool does during
# pio upload, so this merges the bootloader, partition table, and app into
# one image with those already filled in (DIO, 40 MHz, 4 MB — this board).
#
# Also copies the app-only image as app.bin. That is what ArduinoOTA / espota
# send over Wi-Fi; the merged firmware.bin stays for USB (first install and
# USB updates). A future HTTP-from-GitHub OTA would publish app.bin too.
Import("env")

import datetime
import json
import shutil
from pathlib import Path


def stage_flasher(source, target, env):
    build = Path(env.subst("$BUILD_DIR"))
    project = Path(env.subst("$PROJECT_DIR"))
    # One folder per board. All of these chips are plain ESP32, so the
    # installer page has to ask which size rather than guessing from the chip.
    pioenv = env["PIOENV"]
    if pioenv == "cyd40":
        folder, name = "cyd40", "CYDesk 4.0 R"
    elif pioenv == "cyd35":
        folder, name = "cyd35", "CYDesk 3.5"
    else:
        folder, name = "cyd28", "CYDesk 2.8"
    out_dir = project / "flasher" / folder
    out_dir.mkdir(parents=True, exist_ok=True)

    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    boot_app0 = framework / "tools" / "partitions" / "boot_app0.bin"
    app_src = build / "firmware.bin"
    merged = out_dir / "firmware.bin"
    app_out = out_dir / "app.bin"

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
            q(app_src),
        ]
    )
    env.Execute(env.VerboseAction(cmd, "Packing the web flasher image"))
    shutil.copy2(app_src, app_out)

    # The compiler is told to rewrite the home folder, but check the image
    # anyway. A hit here means a path was baked in and must not be committed.
    home = str(Path.home()).encode()
    for path in (merged, app_out):
        if home in path.read_bytes():
            raise SystemExit(
                f"{path.name} still contains this computer's home folder"
            )

    # USB installer: merged image at offset 0. Erase is optional (prompted).
    # Leaving erase off keeps NVS (Wi-Fi + keys) because blank 0xFF gaps in the
    # merged image are skipped when written.
    manifest = {
        "name": name,
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
    print(f"Web flasher ready in flasher/{folder}/ (merged + app.bin)")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", stage_flasher)
