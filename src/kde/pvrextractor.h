#pragma once

#include <KFileMetaData/ExtractorPlugin>

/**
 * File metadata of PowerVR and Sega Dreamcast PVR textures and texture packs, for the Details tab
 * of Dolphin and for Baloo: dimensions, pixel format, bit depth, container, and texture count.
 */
class PvrExtractor : public KFileMetaData::ExtractorPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID kfilemetadata_extractor_iid FILE "pvrextractor.json")
    Q_INTERFACES(KFileMetaData::ExtractorPlugin)

public:
    /**
     * Create the extractor. KFileMetaData creates it through the plugin loader.
     *
     * @param parent Parent object.
     */
    explicit PvrExtractor(QObject *parent = nullptr);

    QStringList mimetypes() const override;
    void extract(KFileMetaData::ExtractionResult *result) override;
};
