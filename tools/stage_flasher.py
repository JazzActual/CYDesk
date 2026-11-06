# After each build, pack the firmware into flasher/ for the browser installer
# and the in-app HTTPS OTA channel.
#
# USB installer (esp-web-tools) gets four separate images — bootloader,
# partition table, otadata (boot_app0), and the app — at their real offsets.
# A single merge_bin image at offset 0 pads the NVS gap (0x9000–0xe000) with
# 0xFF; writing that range erases Wi-Fi and API keys even when "Erase device"
# is unchecked. Listing parts that skip NVS keeps settings on update.
#
# Also copies the app-only image as app.bin and writes channel.json so a
# board on Wi-Fi can discover and pull the matching variant.
Import("env")

import datetime
import hashlib
import json
import shutil
from pathlib import Path

# Same layout as Arduino min_spiffs / huge_app: NVS lives here and must not
# appear in any USB-installer part range.
NVS_START = 0x9000
NVS_END = 0xE000

# ESP32 classic flash map used by this project.
PARTS = (
    ("bootloader.bin", 0x1000),
    ("partitions.bin", 0x8000),
    ("boot_app0.bin", 0xE000),
    ("app.bin", 0x10000),
)


def stage_flasher(source, target, env):
    build = Path(env.subst("$BUILD_DIR"))
    project = Path(env.subst("$PROJECT_DIR"))
    # One folder per board. All of these chips are plain ESP32, so the
    # installer page has to ask which size rather than guessing from the chip.
    pioenv = env["PIOENV"]
    if pioenv == "cyd40c":
        folder, name = "cyd40c", "CYDesk 4.0 C"
    elif pioenv == "cyd40":
        folder, name = "cyd40", "CYDesk 4.0 R"
    elif pioenv == "cyd35c":
        folder, name = "cyd35c", "CYDesk 3.5 C"
    elif pioenv == "cyd35":
        folder, name = "cyd35", "CYDesk 3.5 R"
    elif pioenv == "nicemcu28":
        folder, name = "nicemcu28", "CYDesk 2.8 C"
    else:
        folder, name = "cyd28", "CYDesk 2.8"
    out_dir = project / "flasher" / folder
    out_dir.mkdir(parents=True, exist_ok=True)

    framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    boot_app0_src = framework / "tools" / "partitions" / "boot_app0.bin"
    app_src = build / "firmware.bin"
    boot_src = build / "bootloader.bin"
    part_src = build / "partitions.bin"

    version_file = project / ".pio" / "cydesk_version.txt"
    if version_file.exists():
        version = version_file.read_text().strip()
    else:
        version = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d%H%M")

    copies = {
        "bootloader.bin": boot_src,
        "partitions.bin": part_src,
        "boot_app0.bin": boot_app0_src,
        "app.bin": app_src,
    }
    for name_on_disk, src in copies.items():
        if not src.exists():
            raise SystemExit(f"missing build artifact for flasher: {src}")
        shutil.copy2(src, out_dir / name_on_disk)

    # Drop the old merged image if a previous build left one. It must not be
    # served — writing it wipes NVS.
    merged = out_dir / "firmware.bin"
    if merged.exists():
        merged.unlink()

    # Refuse to ship a part list that would touch NVS.
    for part_name, offset in PARTS:
        size = (out_dir / part_name).stat().st_size
        end = offset + size
        if offset < NVS_END and end > NVS_START:
            raise SystemExit(
                f"{part_name} @ 0x{offset:x}+{size} overlaps NVS "
                f"(0x{NVS_START:x}-0x{NVS_END:x})"
            )

    # The compiler is told to rewrite the home folder, but check the images
    # anyway. A hit here means a path was baked in and must not be committed.
    home = str(Path.home()).encode()
    for part_name, _offset in PARTS:
        path = out_dir / part_name
        if home in path.read_bytes():
            raise SystemExit(
                f"{path.name} still contains this computer's home folder"
            )

    app_out = out_dir / "app.bin"
    app_bytes = app_out.read_bytes()
    sha256 = hashlib.sha256(app_bytes).hexdigest()

    # USB installer: discrete parts. Erase is optional (prompted); leave it
    # off for updates so NVS (Wi-Fi + keys) is never written.
    manifest = {
        "name": name,
        "version": version,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 0,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": [
                    {"path": part_name, "offset": offset}
                    for part_name, offset in PARTS
                ],
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
