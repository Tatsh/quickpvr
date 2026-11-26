#include "registration.h"

#include <array>
#include <string>
#include <utility>

#include <propsys.h>
#include <shlobj.h>

#include "module.h"

namespace QuickPvrShell {

namespace {

constexpr wchar_t kClassesKey[] = L"Software\\Classes\\";
constexpr wchar_t kPreviewHandlersKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers";
constexpr wchar_t kPropertyHandlersKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\";
constexpr wchar_t kExtension[] = L".pvr";
// Shell extension point for thumbnail providers.
constexpr wchar_t kThumbnailHandlerId[] = L"{e357fccd-a995-4576-b01f-234630154e96}";
// Shell extension point for preview handlers.
constexpr wchar_t kPreviewHandlerId[] = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";
// The 64-bit preview handler surrogate host (prevhost.exe).
constexpr wchar_t kPreviewHostAppId[] = L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}";
constexpr wchar_t kThumbnailProviderName[] = L"QuickPVR Thumbnail Provider";
constexpr wchar_t kPreviewHandlerName[] = L"QuickPVR Preview Handler";
constexpr wchar_t kPropertyHandlerName[] = L"QuickPVR Property Handler";
constexpr wchar_t kSchemaFileName[] = L"QuickPVR.propdesc";
constexpr int kGuidStringLength = 39;

// Explorer reads these lists for a file type without a ProgID. FullDetails fills the
// Details tab of Properties, PreviewDetails the details pane, and InfoTip the tooltip.
constexpr wchar_t kFullDetails[] =
    L"prop:System.PropGroup.Image;System.Image.Dimensions;System.Image.HorizontalSize;"
    L"System.Image.VerticalSize;System.Image.BitDepth;System.Image.CompressionText;"
    L"QuickPVR.TextureCount;System.PropGroup.FileSystem;System.ItemNameDisplay;"
    L"System.ItemTypeText;System.ItemFolderPathDisplay;System.Size;System.DateCreated;"
    L"System.DateModified;System.FileAttributes;System.OfflineAvailability;System.OfflineStatus;"
    L"System.SharedWith;System.FileOwner;System.ComputerName";
constexpr wchar_t kPreviewDetails[] =
    L"prop:System.Image.Dimensions;System.Image.BitDepth;System.Image.CompressionText;"
    L"QuickPVR.TextureCount;System.Size;System.DateModified;System.DateCreated";
constexpr wchar_t kInfoTip[] =
    L"prop:System.ItemTypeText;System.Image.Dimensions;System.Image.CompressionText;System.Size;"
    L"System.DateModified";
constexpr std::array kDetailValues = {std::pair{L"FullDetails", kFullDetails},
                                      std::pair{L"PreviewDetails", kPreviewDetails},
                                      std::pair{L"InfoTip", kInfoTip}};

std::wstring guidString(const GUID &guid) {
    std::array<wchar_t, kGuidStringLength> buffer{};
    StringFromGUID2(guid, buffer.data(), static_cast<int>(buffer.size()));
    return buffer.data();
}

HKEY rootFor(Scope scope) {
    return scope == Scope::Machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

HRESULT
setValue(HKEY root, const std::wstring &keyPath, const wchar_t *name, const std::wstring &value) {
    HKEY key = nullptr;
    auto status = RegCreateKeyExW(root,
                                  keyPath.c_str(),
                                  0,
                                  nullptr,
                                  REG_OPTION_NON_VOLATILE,
                                  KEY_SET_VALUE,
                                  nullptr,
                                  &key,
                                  nullptr);
    if (status == ERROR_SUCCESS) {
        status = RegSetValueExW(key,
                                name,
                                0,
                                REG_SZ,
                                reinterpret_cast<const BYTE *>(value.c_str()),
                                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
    return HRESULT_FROM_WIN32(status);
}

std::wstring readValue(HKEY root, const std::wstring &keyPath, const wchar_t *name) {
    std::array<wchar_t, MAX_PATH> buffer{};
    auto size = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    if (RegGetValueW(root, keyPath.c_str(), name, RRF_RT_REG_SZ, nullptr, buffer.data(), &size) !=
        ERROR_SUCCESS) {
        return {};
    }
    return buffer.data();
}

void deleteValue(HKEY root, const std::wstring &keyPath, const wchar_t *name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, keyPath.c_str(), 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, name);
        RegCloseKey(key);
    }
}

HRESULT registerClass(HKEY root,
                      const GUID &clsid,
                      const wchar_t *name,
                      const std::wstring &modulePath,
                      const wchar_t *threadingModel) {
    const auto classKey = std::wstring(kClassesKey) + L"CLSID\\" + guidString(clsid);
    auto result = setValue(root, classKey, nullptr, name);
    if (SUCCEEDED(result)) {
        result = setValue(root, classKey + L"\\InprocServer32", nullptr, modulePath);
    }
    if (SUCCEEDED(result)) {
        result = setValue(root, classKey + L"\\InprocServer32", L"ThreadingModel", threadingModel);
    }
    return result;
}

// Remove an entry only when it still refers to this DLL's class.
void removeKeyIfOurs(HKEY root, const std::wstring &keyPath, const GUID &clsid) {
    if (readValue(root, keyPath, nullptr) == guidString(clsid)) {
        RegDeleteTreeW(root, keyPath.c_str());
    }
}

std::wstring moduleDirectory(const std::wstring &modulePath) {
    return modulePath.substr(0, modulePath.find_last_of(L'\\') + 1);
}

HRESULT registerPropertyHandler(HKEY root, const std::wstring &modulePath) {
    // Explorer may call the property handler from several threads, one call at a time.
    auto result =
        registerClass(root, kPropertyHandlerClsid, kPropertyHandlerName, modulePath, L"Both");
    if (SUCCEEDED(result)) {
        result = setValue(root,
                          std::wstring(kPropertyHandlersKey) + kExtension,
                          nullptr,
                          guidString(kPropertyHandlerClsid));
    }
    const auto associationKey =
        std::wstring(kClassesKey) + L"SystemFileAssociations\\" + kExtension;
    for (const auto &[name, value] : kDetailValues) {
        if (SUCCEEDED(result)) {
            result = setValue(root, associationKey, name, value);
        }
    }
    if (SUCCEEDED(result)) {
        result = PSRegisterPropertySchema((moduleDirectory(modulePath) + kSchemaFileName).c_str());
    }
    return result;
}

void unregisterPropertyHandler(HKEY root, const std::wstring &modulePath) {
    removeKeyIfOurs(root, std::wstring(kPropertyHandlersKey) + kExtension, kPropertyHandlerClsid);
    const auto associationKey =
        std::wstring(kClassesKey) + L"SystemFileAssociations\\" + kExtension;
    for (const auto &[name, value] : kDetailValues) {
        if (readValue(root, associationKey, name) == value) {
            deleteValue(root, associationKey, name);
        }
    }
    RegDeleteTreeW(
        root, (std::wstring(kClassesKey) + L"CLSID\\" + guidString(kPropertyHandlerClsid)).c_str());
    PSUnregisterPropertySchema((moduleDirectory(modulePath) + kSchemaFileName).c_str());
}

std::wstring modulePath() {
    std::array<wchar_t, MAX_PATH> path{};
    const auto length = GetModuleFileNameW(moduleHandle(), path.data(), MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return {};
    }
    return path.data();
}

} // namespace

HRESULT registerServer(Scope scope) {
    const auto path = modulePath();
    if (path.empty()) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    const auto root = rootFor(scope);
    const auto shellExtensionKey = std::wstring(kClassesKey) + kExtension + L"\\ShellEx\\";

    auto result =
        registerClass(root, kThumbnailProviderClsid, kThumbnailProviderName, path, L"Apartment");
    if (SUCCEEDED(result)) {
        result = registerClass(root, kPreviewHandlerClsid, kPreviewHandlerName, path, L"Apartment");
    }
    if (SUCCEEDED(result)) {
        result = setValue(root,
                          std::wstring(kClassesKey) + L"CLSID\\" + guidString(kPreviewHandlerClsid),
                          L"AppID",
                          kPreviewHostAppId);
    }
    if (SUCCEEDED(result)) {
        result = setValue(root,
                          shellExtensionKey + kThumbnailHandlerId,
                          nullptr,
                          guidString(kThumbnailProviderClsid));
    }
    if (SUCCEEDED(result)) {
        result = setValue(
            root, shellExtensionKey + kPreviewHandlerId, nullptr, guidString(kPreviewHandlerClsid));
    }
    if (SUCCEEDED(result)) {
        result = setValue(root,
                          kPreviewHandlersKey,
                          guidString(kPreviewHandlerClsid).c_str(),
                          kPreviewHandlerName);
    }
    if (SUCCEEDED(result) && scope == Scope::Machine) {
        result = registerPropertyHandler(root, path);
    }
    if (SUCCEEDED(result)) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    return result;
}

HRESULT unregisterServer(Scope scope) {
    const auto root = rootFor(scope);
    const auto shellExtensionKey = std::wstring(kClassesKey) + kExtension + L"\\ShellEx\\";
    removeKeyIfOurs(root, shellExtensionKey + kThumbnailHandlerId, kThumbnailProviderClsid);
    removeKeyIfOurs(root, shellExtensionKey + kPreviewHandlerId, kPreviewHandlerClsid);
    for (const auto &clsid : {kThumbnailProviderClsid, kPreviewHandlerClsid}) {
        const auto classKey = std::wstring(kClassesKey) + L"CLSID\\" + guidString(clsid);
        RegDeleteTreeW(root, classKey.c_str());
    }
    deleteValue(root, kPreviewHandlersKey, guidString(kPreviewHandlerClsid).c_str());
    if (scope == Scope::Machine) {
        unregisterPropertyHandler(root, modulePath());
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

} // namespace QuickPvrShell
