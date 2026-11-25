#include "thumbnailprovider.h"

#include <utility>

#include <shlwapi.h>

#include "shellimage.h"

namespace QuickPvrShell {

IFACEMETHODIMP ThumbnailProvider::QueryInterface(REFIID iid, void **object) {
    static const QITAB interfaces[] = {
        QITABENT(ThumbnailProvider, IInitializeWithStream),
        QITABENT(ThumbnailProvider, IThumbnailProvider),
        {nullptr, 0},
    };
    return QISearch(this, interfaces, iid, object);
}

IFACEMETHODIMP_(ULONG) ThumbnailProvider::AddRef() {
    return addReference();
}

IFACEMETHODIMP_(ULONG) ThumbnailProvider::Release() {
    return release();
}

IFACEMETHODIMP ThumbnailProvider::Initialize(IStream *stream, DWORD) {
    if (initialized_) {
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    }
    auto data = readStream(stream);
    if (!data) {
        return data.error();
    }
    data_ = std::move(*data);
    initialized_ = true;
    return S_OK;
}

IFACEMETHODIMP
ThumbnailProvider::GetThumbnail(UINT size, HBITMAP *bitmap, WTS_ALPHATYPE *alphaType) {
    if (bitmap == nullptr || alphaType == nullptr) {
        return E_POINTER;
    }
    *bitmap = nullptr;
    if (!initialized_) {
        return E_UNEXPECTED;
    }
    const auto texture = decodeTexture(data_);
    if (!texture) {
        return texture.error();
    }
    // Explorer requests a square bound; the thumbnail retains the texture's aspect ratio.
    const auto scaled = scaleToFit(texture->image, size, size);
    *bitmap = createBitmap(scaled);
    if (*bitmap == nullptr) {
        return E_OUTOFMEMORY;
    }
    *alphaType = WTSAT_ARGB;
    return S_OK;
}

} // namespace QuickPvrShell
