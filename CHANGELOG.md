<!-- markdownlint-configure-file {"MD024": { "siblings_only": true } } -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.1/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [unreleased]

### Added

- Sega Dreamcast `PVRT` textures, with or without `GBIX`: ARGB1555, RGB565, ARGB4444, RGB555,
  YUV422, and bump-map texels in twiddled, twiddled rectangle, rectangle, stride, VQ, and small VQ
  layouts with or without mipmaps, and 4 and 8 bpp palettised textures with `PVPL` palettes.
- Dreamcast texture packs, such as the `*_T.PVR` files of _Incoming_, previewed as a contact sheet.
- PVR v2 support for every documented uncompressed pixel type, header masks for other types, DXT1
  to DXT5, twiddled data, the vertical-flip flag, and version 1 headers.
- PVR v3 support for any uncompressed channel layout, BC1 to BC5, UYVY, YUY2, RGBG8888, GRGB8888, orientation
  metadata, and premultiplied alpha.
- QuickPVR.app, a `.pvr` viewer with QuickLook preview and thumbnail app extensions for macOS 12
  and later.
- The `pvr2png` converter and its manual page. It writes PNG files with stb_image_write.
- A Qt 6 image format plugin, for Gwenview and other Qt applications.
- A KIO thumbnailer, for Dolphin.
- The `image/x-pvr` MIME type for freedesktop systems.
- An Explorer thumbnail provider and preview handler for Windows, registered per user.
- Unit tests, run with `ctest`.
- Release packages: a disk image of QuickPVR.app, an installer package for the legacy generator,
  and a `pvr2png` archive for macOS; NSIS installers and archives for Windows on x86-64 and Arm;
  and `pvr2png` as an AppImage, a Flatpak, and a snap for Linux on x86-64 and aarch64.

### Changed

- libpvr is rewritten in C++23. It checks every read against the data, reports failures through
  `std::expected`, and rejects oversized textures.
- The legacy generator uses the new decoder and is embedded in QuickPVR.app.
- PVR v2 textures are flipped only when the vertical-flip flag is set (previously always).
- The build uses CMake. The Xcode project is removed.

### Fixed

- PVRTC decoding no longer fails an assertion in debug builds.

### Removed

- The Qt 5 viewer (`qtpvr`), replaced by QuickPVR.app and `pvr2png`.

[unreleased]: https://github.com/Tatsh/quickpvr/commits/master
