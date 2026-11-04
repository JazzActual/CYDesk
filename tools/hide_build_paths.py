# Loaded before the build. Rewrites this computer's home folder to a fixed
# prefix inside __FILE__ and debug info, so a firmware image never contains
# the account name of whoever built it.
Import("env")

from pathlib import Path

home = Path.home().as_posix()
env.Append(
    CCFLAGS=[
        f"-ffile-prefix-map={home}=/build",
        f"-fmacro-prefix-map={home}=/build",
        f"-fdebug-prefix-map={home}=/build",
    ]
)
