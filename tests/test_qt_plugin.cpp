#include <cstdlib>
#include <iostream>

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QImageReader>

namespace {

int failures = 0;

void check(bool condition, const char *description) {
    std::cout << (condition ? "ok   " : "FAIL ") << description << '\n';
    if (!condition) {
        ++failures;
    }
}

QString sample(const char *name) {
    return QStringLiteral(PVR_SAMPLES_DIR "/") + QString::fromUtf8(name);
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::addLibraryPath(QStringLiteral(PVR_PLUGIN_DIR));

    check(QImageReader::supportedImageFormats().contains("pvr"), "the pvr format is registered");
    check(QImageReader::supportedMimeTypes().contains("image/x-pvr"),
          "the image/x-pvr MIME type is registered");

    QImageReader banner(sample("brighterday.pvr"));
    // The reader forgets the format once it has read the image.
    check(banner.format() == "pvr", "the format is detected");
    const auto bannerImage = banner.read();
    check(bannerImage.size() == QSize(256, 64), "a PVR v2 banner is read at 256x64");

    const QImage pack(sample("dreamcast/PACK_T.PVR"));
    check(pack.width() > 256 && pack.height() == 256, "a Dreamcast pack is read as a sheet");

    const QImage palettized(sample("dreamcast/logo_pal8.pvr"));
    check(palettized.size() == QSize(128, 128), "a palettized Dreamcast texture is read");

    const QImage headerless(sample("pvr2/apple_4bpp.pvr"));
    check(headerless.size() == QSize(256, 256), "headerless PVRTC named *.pvr is read");

    QFile source(sample("dreamcast/logo_argb4444_twiddled.pvr"));
    source.open(QIODevice::ReadOnly);
    auto contents = source.readAll();
    QBuffer renamed(&contents);
    renamed.open(QIODevice::ReadOnly);
    QImageReader byContent(&renamed);
    check(byContent.format() == "pvr", "a texture without a .pvr name is detected by content");
    check(byContent.read().size() == QSize(256, 256), "a texture is read from a buffer");

    // Headerless PVRTC is recognised by size alone. Other data of that size is not detected.
    QByteArray powerOfTwo(4096, '\x5a');
    QBuffer buffer(&powerOfTwo);
    buffer.open(QIODevice::ReadOnly);
    QImageReader unnamed(&buffer);
    check(unnamed.format() != "pvr", "data that is not a named .pvr file is not detected");

    QFile notTexture(QStringLiteral(PVR_SAMPLES_DIR "/../CMakeLists.txt"));
    QImageReader text(&notTexture, "pvr");
    check(text.read().isNull(), "a file that is not a texture fails to read");

    std::cout << (failures == 0 ? "all passed\n" : "failures\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
