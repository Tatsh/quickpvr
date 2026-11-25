#pragma once

#include <cstdint>
#include <vector>

#include <windows.h>

#include <propsys.h>
#include <thumbcache.h>

#include "module.h"

namespace QuickPvrShell {

/** Explorer thumbnails of PowerVR and Sega Dreamcast PVR textures and texture packs. */
class ThumbnailProvider final : public IInitializeWithStream, public IThumbnailProvider, ComObject {
public:
    IFACEMETHODIMP QueryInterface(REFIID iid, void **object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream *stream, DWORD mode) override;
    IFACEMETHODIMP GetThumbnail(UINT size, HBITMAP *bitmap, WTS_ALPHATYPE *alphaType) override;

private:
    std::vector<std::uint8_t> data_;
    bool initialized_{};
};

} // namespace QuickPvrShell
