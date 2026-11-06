# Pre-build: stamp a comparable firmware version into the binary and into
# .pio/cydesk_version.txt so stage_flasher.py can publish the same id.
#
# Format: UTC YYYYMMDDHHMM (lexicographic compare = chronological).
Import("env")

from datetime import datetime, timezone
from pathlib import Path

version = datetime.now(timezone.utc).strftime("%Y%m%d%H%M")
stamp_dir = Path(env.subst("$PROJECT_DIR")) / ".pio"
stamp_dir.mkdir(parents=True, exist_ok=True)
(stamp_dir / "cydesk_version.txt").write_text(version + "\n")

# CPPDEFINES value with embedded quotes for a string macro.
env.Append(CPPDEFINES=[("CYDESK_FW_VERSION", '\\"' + version + '\\"')])
print(f"Firmware version {version}")
