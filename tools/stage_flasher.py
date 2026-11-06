# After each build, pack the firmware into flasher/ for the browser installer
# and the in-app HTTPS OTA channel.
#
# ESP Web Tools cannot patch flash mode and size the way esptool does during
# pio upload, so this merges the bootloader, partition table, and app into
# one image with those already filled in (DIO, 40 MHz, 4 MB — this board).
#
# Also copies the app-only image as app.bin and writes channel.json so a
# board on Wi-Fi can discover and pull the matching variant.
Import("env")

import datetime
import hashlib
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

    version_file = project / ".pio" / "cydesk_version.txt"
    if version_file.exists():
        version = version_file.read_text().strip()
    else:
        version = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d%H%M")

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

    app_bytes = app_out.read_bytes()
    sha256 = hashlib.sha256(app_bytes).hexdigest()

    # USB installer: merged image at offset 0. Erase is optional (prompted).
    # Leaving erase off keeps NVS (Wi-Fi + keys) because blank 0xFF gaps in the
    # merged image are skipped when written.
    manifest = {
        "name": name,
        "version": version,
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

    # In-app OTA channel. Boards fetch this over HTTPS from GitHub Pages.
    channel = {
        "name": name,
        "board": folder,
        "version": version,
        "app": "app.bin",
        "size": len(app_bytes),
        "sha256": sha256,
    }
    (out_dir / "channel.json").write_text(json.dumps(channel, indent=2) + "\n")
    print(f"Web flasher + OTA channel ready in flasher/{folder}/ (version {version})")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", stage_flasher)
