#pragma once

#include <KIO/ThumbnailCreator>

/** Dolphin and KIO thumbnails of PowerVR and Sega Dreamcast PVR textures and texture packs. */
class PvrThumbnailCreator : public KIO::ThumbnailCreator {
    Q_OBJECT

public:
    /**
     * Create the thumbnailer. KIO calls the constructor through the plugin factory.
     *
     * @param parent Parent object.
     * @param args Plugin arguments, unused.
     */
    PvrThumbnailCreator(QObject *parent, const QVariantList &args);

    KIO::ThumbnailResult create(const KIO::ThumbnailRequest &request) override;
};
