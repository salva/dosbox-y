# DOSBox-X Ubuntu Package Installation

1. Update the package information:

   sudo apt-get update

2. Install the DOSBox-X Debian package:

   sudo apt-get install ./dosbox-x_<version>nightly_amd64.deb

Replace `<version>` with the version of the downloaded package.

For example:

```
sudo apt-get install ./dosbox-x_20260910nightly_amd64.deb
```

APT will automatically install any required dependencies.

3. The CI package provides the SDL2 executable, `dosbox-x-sdl2`, launched by the
   `dosbox-x` command.

SDL1 source remains in the repository, but it is not built, tested or included in
these CI packages.

4. Start the default SDL2 version:

   dosbox-x

The installed SDL2 executable can also be launched directly:

```
dosbox-x-sdl2
```
