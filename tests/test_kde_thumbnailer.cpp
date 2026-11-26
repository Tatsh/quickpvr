#include <cstdlib>
#include <iostream>

#include <KIO/ThumbnailCreator>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QCoreApplication>
#include <QImage>
#include <QUrl>

namespace {

int failures = 0;

void check(bool condition, const char *description) {
    std::cout << (condition ? "ok   " : "FAIL ") << description << '\n';
    if (!condition) {
        ++failures;
    }
}

QUrl sample(const char *name) {
    return QUrl::fromLocalFile(QStringLiteral(PVR_SAMPLES_DIR "/") + QString::fromUtf8(name));
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::addLibraryPath(QStringLiteral(PVR_PLUGIN_DIR));

    // Dolphin finds thumbnailers with the same lookup and lists them under Previews.
    const auto metaData = KPluginMetaData::findPluginById(QStringLiteral("kf6/thumbcreator"),
                                                          QStringLiteral("pvrthumbnail"));
    check(metaData.isValid(), "the thumbnailer is found in kf6/thumbcreator");
    check(metaData.mimeTypes().contains(QStringLiteral("image/x-pvr")),
          "the thumbnailer handles image/x-pvr");
    check(!metaData.name().isEmpty(), "the thumbnailer has a name for the Previews list");

    const auto result = KPluginFactory::instantiatePlugin<KIO::ThumbnailCreator>(metaData);
    check(result.plugin != nullptr, "the thumbnailer is created by its factory");
    if (result.plugin == nullptr) {
        std::cout << result.errorString.toStdString() << '\n';
        std::cout << "failures\n";
        return EXIT_FAILURE;
    }

    const auto thumbnail = result.plugin->create(KIO::ThumbnailRequest(
        sample("brighterday.pvr"), QSize(128, 128), QStringLiteral("image/x-pvr"), 1.0, 0.0f));
    check(thumbnail.isValid(), "a thumbnail of a PVR v2 banner is created");
    check(thumbnail.image().size() == QSize(128, 32), "the thumbnail preserves the aspect ratio");

    const auto missing = result.plugin->create(KIO::ThumbnailRequest(
        sample("missing.pvr"), QSize(128, 128), QStringLiteral("image/x-pvr"), 1.0, 0.0f));
    check(!missing.isValid(), "a missing file fails");

    delete result.plugin;
    std::cout << (failures == 0 ? "all passed\n" : "failures\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
