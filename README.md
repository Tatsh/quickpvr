# QuickPVR

<!-- WISWA-GENERATED-README:START -->
<!-- WISWA-GENERATED-README:STOP -->

Finder previews and thumbnails, a viewer, and a command-line converter for PowerVR and Sega
Dreamcast PVR textures on macOS, Dolphin thumbnails and Gwenview support on Linux, and Explorer
thumbnails and previews on Windows.

QuickPVR began as [Limbic Software's QuickLook plugin](https://github.com/Volcore/quickpvr) for
iPhone-era PVR files. This fork adds a bounds-checked decoder for many more formats, including the
Dreamcast textures and texture packs documented by [Dade](https://github.com/Tatsh/dade), modern
QuickLook extensions, `pvr2png`, and KDE integration.

## Contents

- **QuickPVR.app** includes:
  - QuickLook preview and thumbnail app extensions for macOS 12 and later, including macOS 15 and
    later, where legacy generators are no longer loaded.
  - The legacy QuickPVR.qlgenerator in `Contents/Library/QuickLook`, used by macOS 14 and earlier.
  - A viewer. Open a `.pvr` file in the app to see it with its format details.
- **pvr2png** converts textures to PNG files on every platform. See `man pvr2png`.
- **A Qt image format plugin** (`pvr`) lets Gwenview and other Qt applications open the textures.
- **A KIO thumbnailer** shows the textures as thumbnails in Dolphin and other KDE file views.
- **The `image/x-pvr` MIME type** identifies the files on freedesktop desktops.
- **QuickPVRShell.dll**, an Explorer thumbnail provider and preview handler for Windows.
- **libpvr** is the static C++23 library behind all of them.

A texture pack is shown as a contact sheet of all its textures. Small textures are enlarged
without smoothing so their texels remain sharp.

## Supported formats

| Container                                       | Formats                                                                                                                                                                |
| ----------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| PowerVR legacy (v1 and v2, `PVR!`)              | 4444, 1555, 5551, 565, 555, 888, 8888, 8332, BGRA8888, RGB332, I8, AI88, A8, AL44, PVRTC 2 and 4 bpp, and DXT1 to DXT5. Other uncompressed types use the header masks. |
| PowerVR v3 (`PVR\x03`)                          | Any uncompressed channel layout (integer, normalised, signed, half float, and float), PVRTC 2 and 4 bpp, BC1 to BC5, UYVY, YUY2, RGBG8888, and GRGB8888.               |
| Apple `texturetool` PVRTC without a header      | Square power-of-two textures at 2 and 4 bpp.                                                                                                                           |
| Sega Dreamcast `PVRT`, optionally after `GBIX`  | ARGB1555, RGB565, ARGB4444, RGB555, YUV422, and bump maps in twiddled, twiddled rectangle, rectangle, stride, VQ, and small VQ layouts, with or without mipmaps.       |
| Sega Dreamcast palettised `PVRT`                | 4 and 8 bpp indices with a `.pvp` palette.                                                                                                                             |
| Dreamcast texture packs (for example `*_T.PVR`) | A table of `(offset, size)` pairs pointing at `PVRT` chunks, including the overstated last entry of _Incoming_ packs.                                                  |

Twiddled PVR v2 data, the PVR v2 vertical-flip flag, PVR v3 orientation metadata, and premultiplied
alpha are honoured. ETC1, ETC2, EAC, PVRTC-II, BC6H, and BC7 are not decoded.

A palettised Dreamcast texture needs its palette (`.pvp`) for colour. `pvr2png` and the legacy
generator use a `.pvp` with the same name automatically. The sandboxed extensions can read only the
texture and show palette indices as greys.

Some PVR v2 files written for OpenGL, including the Limbic samples in `pvrsamples/pvr2`, are stored
bottom-up without the vertical-flip flag and appear upside down. The header does not
distinguish them from files stored top-down.

## Downloads

Each [release](https://github.com/Tatsh/quickpvr/releases) has these packages:

| Package                                        | Contents                                                       |
| ---------------------------------------------- | -------------------------------------------------------------- |
| `QuickPVR-*-macos-universal-Release.dmg`       | QuickPVR.app with its QuickLook extensions, for macOS 12+.     |
| `quickpvr-*-macos-universal-*-qlgenerator.pkg` | The legacy generator in `/Library/QuickLook`, for macOS 12–14. |
| `quickpvr-*-macos-universal-Release.tar.gz`    | `pvr2png` and its manual page.                                 |
| `quickpvr-*-windows-*-Release-setup.exe`       | `pvr2png` and the Explorer handlers, for all or one user.      |
| `quickpvr-*-windows-*-Release.zip`             | The same files, to register with `regsvr32` by hand.           |
| `quickpvr-*-appimage-*-Release.AppImage`       | `pvr2png` for Linux on x86-64 and aarch64.                     |
| `quickpvr-*-flatpak-*-Release.flatpak`         | `pvr2png` as the Flatpak `sh.tat.pvr2png`.                     |
| `quickpvr-*-snap-*-Release.snap`               | `pvr2png` as the snap `pvr2png`.                               |

The packages are not notarised or signed by a known publisher, so macOS and Windows warn before
opening them. Install a downloaded snap with `snap install --dangerous`. The packages ending in
`-symbols` or `-debug` hold the debugging information.

On Linux, the Qt plugin and the Dolphin thumbnailer are not in the release packages. Install them
from a distribution package or [build them](#building).

## Building

Building requires CMake 3.25 or later and a C++23 compiler with `std::expected` (Clang 17, GCC 13, or
later). `pvr2png` also needs spdlog, argparse, and stb, each downloaded when it is not installed.
The macOS parts need the macOS 12 SDK or later. The Qt plugin needs Qt 6, and the thumbnailer needs
KDE Frameworks 6 (KIO and KCoreAddons) and Extra CMake Modules.

```shell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix /usr/local
```

| Option                            | Default                 | Description                                                         |
| --------------------------------- | ----------------------- | ------------------------------------------------------------------- |
| `QUICKPVR_BUILD_CLI`              | `ON`                    | Build `pvr2png`.                                                    |
| `QUICKPVR_BUILD_QUICKLOOK`        | `ON` on macOS           | Build QuickPVR.app with the QuickLook extensions.                   |
| `QUICKPVR_BUILD_QLGENERATOR`      | `ON` on macOS           | Build the legacy generator, embedded in the app.                    |
| `QUICKPVR_CODESIGN_IDENTITY`      | `-`                     | Signing identity for the bundles (`-` is ad hoc).                   |
| `QUICKPVR_INSTALL_APPDIR`         | `<prefix>/Applications` | Where QuickPVR.app is installed.                                    |
| `QUICKPVR_BUILD_QT_PLUGIN`        | `ON` on Linux and BSD   | Build the Qt image format plugin.                                   |
| `QUICKPVR_BUILD_KDE_THUMBNAILER`  | `ON` on Linux and BSD   | Build the KIO thumbnailer for Dolphin.                              |
| `QUICKPVR_INSTALL_GWENVIEW_ENTRY` | `ON` on Linux and BSD   | Install a hidden desktop entry that opens `.pvr` files in Gwenview. |
| `QUICKPVR_BUILD_WINDOWS_SHELL`    | `ON` on Windows         | Build the Explorer thumbnail and preview DLL.                       |
| `BUILD_DOCS`                      | `OFF`                   | Build the API documentation with Doxygen.                           |

## Using QuickPVR on Linux

Configure with the prefix of Qt, normally `-DCMAKE_INSTALL_PREFIX=/usr`. The thumbnailer goes in the
plugin directory of Qt (for example `/usr/lib64/qt6/plugins/kf6/thumbcreator`) only when the prefix
matches. Changing the prefix with `cmake --install --prefix` does not move the thumbnailer.

Install with `cmake --install`, then register the `image/x-pvr` type by running
`update-mime-database` on the `mime` directory the files went to (for example
`update-mime-database /usr/share/mime`). Also run `update-desktop-database` on the `applications`
directory (for example `update-desktop-database /usr/share/applications`). Distribution packages
usually run both commands.

Gwenview then opens `.pvr` files, including on a double-click in Dolphin. In Dolphin, enable
PowerVR and Dreamcast textures under Configure Dolphin, Interface, and Previews.

## Using the QuickLook extensions

Install QuickPVR.app, for example in `/Applications`, and open it once. If previews do not appear,
enable the QuickPVR extensions in System Settings, under General, Login Items & Extensions, and
Quick Look (or under Privacy & Security and Extensions on macOS 13 and 14).

## Using QuickPVR on Windows

Build with Visual Studio 2022 or MinGW-w64 (GCC 13 or later). To cross-compile with MinGW-w64, pass
`-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake`.

The installer offers to install for everyone or only for the current user. Explorer then shows
thumbnails of `.pvr` files and previews them in the preview pane. Installing for everyone needs
administrator rights and also adds the dimensions, bit depth, pixel format, and texture count to
the Details tab of Properties and to the details pane. Windows loads property handlers only when
they are registered for every user. For a silent installation, pass `/S` with `/AllUsers` or
`/CurrentUser`.

To register the DLL by hand, place `QuickPVR.propdesc` next to it and run
`regsvr32 /n /i:machine QuickPVRShell.dll` as an administrator for every user, or
`regsvr32 QuickPVRShell.dll` for the current user. Add `/u` to remove the registration. Restart
Explorer if thumbnails of files already viewed do not change.

## Licence

QuickPVR is under the BSD 3-Clause licence. See [LICENSE.txt](LICENSE.txt). The PVRTC decompressor
is under the zlib licence.
