#include "propertyhandler.h"

#include <cstdint>
#include <string>

// initguid.h makes propkey.h define the property keys instead of only declaring them.
#include <initguid.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>

#include "pvr/pvr.h"
#include "shellimage.h"

namespace QuickPvrShell {

namespace {

constexpr PROPERTYKEY kTextureCountKey{kPropertyFormatId, kTextureCountPropertyId};

HRESULT storeNumber(IPropertyStoreCache *cache, REFPROPERTYKEY key, std::uint32_t number) {
    PROPVARIANT value;
    auto result = InitPropVariantFromUInt32(number, &value);
    if (SUCCEEDED(result)) {
        result = cache->SetValueAndState(key, &value, PSC_NORMAL);
        PropVariantClear(&value);
    }
    return result;
}

HRESULT storeText(IPropertyStoreCache *cache, REFPROPERTYKEY key, const std::string &text) {
    const auto wide = widen(text);
    PROPVARIANT value;
    auto result = InitPropVariantFromString(wide.c_str(), &value);
    if (SUCCEEDED(result)) {
        result = cache->SetValueAndState(key, &value, PSC_NORMAL);
        PropVariantClear(&value);
    }
    return result;
}

HRESULT storeDetails(IPropertyStoreCache *cache, const Pvr::Details &details) {
    auto result = S_OK;
    if (details.width && details.height) {
        result = storeNumber(cache, PKEY_Image_HorizontalSize, *details.width);
        if (SUCCEEDED(result)) {
            result = storeNumber(cache, PKEY_Image_VerticalSize, *details.height);
        }
        if (SUCCEEDED(result)) {
            result =
                storeText(cache,
                          PKEY_Image_Dimensions,
                          std::to_string(*details.width) + " x " + std::to_string(*details.height));
        }
    }
    if (SUCCEEDED(result) && details.bitsPerPixel.value_or(0) != 0) {
        result = storeNumber(cache, PKEY_Image_BitDepth, *details.bitsPerPixel);
    }
    if (SUCCEEDED(result)) {
        auto text = std::string(Pvr::containerName(details.container));
        if (!details.format.empty()) {
            text += ", " + details.format;
        }
        result = storeText(cache, PKEY_Image_CompressionText, text);
    }
    if (SUCCEEDED(result) && details.container == Pvr::Container::DreamcastPack) {
        result =
            storeNumber(cache, kTextureCountKey, static_cast<std::uint32_t>(details.textureCount));
    }
    return result;
}

} // namespace

PropertyHandler::~PropertyHandler() {
    if (cache_ != nullptr) {
        cache_->Release();
    }
}

IFACEMETHODIMP PropertyHandler::QueryInterface(REFIID iid, void **object) {
    static const QITAB interfaces[] = {
        QITABENT(PropertyHandler, IInitializeWithStream),
        QITABENT(PropertyHandler, IPropertyStore),
        QITABENT(PropertyHandler, IPropertyStoreCapabilities),
        {nullptr, 0},
    };
    return QISearch(this, interfaces, iid, object);
}

IFACEMETHODIMP_(ULONG) PropertyHandler::AddRef() {
    return addReference();
}

IFACEMETHODIMP_(ULONG) PropertyHandler::Release() {
    return release();
}

IFACEMETHODIMP PropertyHandler::Initialize(IStream *stream, DWORD) {
    if (cache_ != nullptr) {
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    }
    const auto data = readStream(stream);
    if (!data) {
        return data.error();
    }
    const auto result = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&cache_));
    if (FAILED(result)) {
        return result;
    }
    // A file that does not decode has only the properties the file system provides.
    const auto decoded = Pvr::decode(*data);
    if (!decoded) {
        return S_OK;
    }
    return storeDetails(cache_, Pvr::describe(*decoded));
}

IFACEMETHODIMP PropertyHandler::GetCount(DWORD *count) {
    if (count == nullptr) {
        return E_POINTER;
    }
    *count = 0;
    return cache_ != nullptr ? cache_->GetCount(count) : E_UNEXPECTED;
}

IFACEMETHODIMP PropertyHandler::GetAt(DWORD index, PROPERTYKEY *key) {
    if (key == nullptr) {
        return E_POINTER;
    }
    return cache_ != nullptr ? cache_->GetAt(index, key) : E_UNEXPECTED;
}

IFACEMETHODIMP PropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT *value) {
    if (value == nullptr) {
        return E_POINTER;
    }
    PropVariantInit(value);
    return cache_ != nullptr ? cache_->GetValue(key, value) : E_UNEXPECTED;
}

IFACEMETHODIMP PropertyHandler::SetValue(REFPROPERTYKEY, REFPROPVARIANT) {
    return STG_E_ACCESSDENIED;
}

IFACEMETHODIMP PropertyHandler::Commit() {
    return STG_E_ACCESSDENIED;
}

IFACEMETHODIMP PropertyHandler::IsPropertyWritable(REFPROPERTYKEY) {
    return S_FALSE;
}

} // namespace QuickPvrShell
