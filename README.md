# WiiSaber

A minimal Nintendo Wii homebrew starter project using devkitPPC and libogc.
It builds `wiisaber.dol` and displays a text screen until START is pressed on
the first connected GameCube controller.

## Requirements

- devkitPro with the `devkitPPC` toolchain
- libogc from the devkitPro Wii development package group

On a devkitPro Linux installation, initialize the environment and build with:

```sh
source /etc/profile.d/devkit-env.sh
make
```

The resulting `wiisaber.dol` can be run from an SD card with a Wii homebrew
loader. To use the Homebrew Channel's application browser, add `meta.xml` and
an `icon.png` alongside the executable in an app folder.

Clean generated files with `make clean`.
