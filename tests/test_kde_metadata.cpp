#include <cstdlib>
#include <iostream>

#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/Properties>
#include <KFileMetaData/SimpleExtractionResult>
#include <QCoreApplication>

namespace {

namespace Property = KFileMetaData::Property;

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

// Run every extractor for image/x-pvr over a file, as Dolphin does for its Details tab.
KFileMetaData::SimpleExtractionResult extract(KFileMetaData::ExtractorCollection &collection,
                                              const QString &path) {
    const auto mimeType = QStringLiteral("image/x-pvr");
    KFileMetaData::SimpleExtractionResult result(path, mimeType);
    for (auto *extractor : collection.fetchExtractors(mimeType)) {
        extractor->extract(&result);
    }
    return result;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::addLibraryPath(QStringLiteral(PVR_PLUGIN_DIR));

    KFileMetaData::ExtractorCollection collection;
    check(!collection.fetchExtractors(QStringLiteral("image/x-pvr")).isEmpty(),
          "an extractor is found for image/x-pvr");

    const auto texture = extract(collection, sample("dreamcast/logo_argb4444_twiddled.pvr"));
    const auto &properties = texture.properties();
    check(properties.value(Property::Width).toInt() == 256 &&
              properties.value(Property::Height).toInt() == 256,
          "a texture is 256x256");
    check(properties.value(Property::Description).toString() == QStringLiteral("Dreamcast PVR"),
          "the description is the container name");
    check(properties.value(Property::PixelFormat).toString().contains(QStringLiteral("ARGB4444")) &&
              properties.value(Property::PixelFormat).toString().endsWith(QStringLiteral("16 bpp")),
          "the pixel format includes the bit depth");

    const auto pack = extract(collection, sample("dreamcast/PACK_T.PVR"));
    check(pack.properties().value(Property::Description).toString() ==
              QStringLiteral("Dreamcast PVR pack, 2 textures"),
          "the description of a pack includes the texture count");
    check(pack.properties()
              .value(Property::PixelFormat)
              .toString()
              .contains(QStringLiteral("256x64")),
          "the pixel format of a pack lists each texture");

    const auto text = extract(collection, sample("../CMakeLists.txt"));
    check(!text.properties().contains(Property::Width),
          "a file that is not a texture does not have dimensions");

    std::cout << (failures == 0 ? "all passed\n" : "failures\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
