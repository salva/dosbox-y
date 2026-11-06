# DOSBox-X AppImage

This AppImage contains the SDL2 version of DOSBox-X only. SDL1 source remains in
the repository, but it is not built, tested or included in CI artifacts.

## Requirements

A 64-bit Linux system is required.

The AppImage is built on Ubuntu 22.04. It is intended for use on modern 64-bit Linux distributions.

## Running DOSBox-X

Make the AppImage executable if necessary:

```bash
chmod +x dosbox-x_*nightly_amd64.AppImage
```

Run the SDL2 version:

```bash
./dosbox-x_*nightly_amd64.AppImage
```

Options other than `--appimage-help` are passed directly to the SDL2 executable.
For example:

```bash
./dosbox-x_*nightly_amd64.AppImage -fullscreen
```

runs the SDL2 version with the `-fullscreen` option.

## AppImage-specific options

To display the AppImage launcher help:

```bash
./dosbox-x_*nightly_amd64.AppImage --appimage-help
```

This displays:

* `--appimage-help` — Display AppImage-specific help.

The normal DOSBox-X `--help` option is passed to DOSBox-X:

```bash
./dosbox-x_*nightly_amd64.AppImage --help
```

## DOSBox-X options

Except for the AppImage-specific `--appimage-help` option, command-line options are passed directly to DOSBox-X.

For more information about DOSBox-X options and configuration, see the included reference configuration files.
