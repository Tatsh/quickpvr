#include "registration.h"

#include <array>
#include <string>

#include <shlobj.h>

#include "module.h"

namespace QuickPvrShell {

namespace {

constexpr wchar_t kClassesKey[] = L"Software\\Classes\\";
constexpr wchar_t kPreviewHandlersKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers";
constexpr wchar_t kExtension[] = L".pvr";
// Shell extension point for thumbnail providers.
constexpr wchar_t kThumbnailHandlerId[] = L"{e357fccd-a995-4576-b01f-234630154e96}";
// Shell extension point for preview handlers.
constexpr wchar_t kPreviewHandlerId[] = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";
// The 64-bit preview handler surrogate host (prevhost.exe).
constexpr wchar_t kPreviewHostAppId[] = L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}";
constexpr wchar_t kThumbnailProviderName[] = L"QuickPVR Thumbnail Provider";
constexpr wchar_t kPreviewHandlerName[] = L"QuickPVR Preview Handler";
constexpr int kGuidStringLength = 39;

std::wstring guidString(const GUID &guid) {
    std::array<wchar_t, kGuidStringLength> buffer{};
    StringFromGUID2(guid, buffer.data(), static_cast<int>(buffer.size()));
    return buffer.data();
}

HRESULT setValue(const std::wstring &keyPath, const wchar_t *name, const std::wstring &value) {
    HKEY key = nullptr;
    auto status = RegCreateKeyExW(HKEY_CURRENT_USER,
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

std::wstring readValue(const std::wstring &keyPath, const wchar_t *name) {
    std::array<wchar_t, MAX_PATH> buffer{};
    auto size = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER,
                     keyPath.c_str(),
                     name,
                     RRF_RT_REG_SZ,
                     nullptr,
                     buffer.data(),
                     &size) != ERROR_SUCCESS) {
        return {};
    }
    return buffer.data();
}

HRESULT registerClass(const GUID &clsid, const wchar_t *name, const std::wstring &modulePath) {
    const auto classKey = std::wstring(kClassesKey) + L"CLSID\\" + guidString(clsid);
    auto result = setValue(classKey, nullptr, name);
    if (SUCCEEDED(result)) {
        result = setValue(classKey + L"\\InprocServer32", nullptr, modulePath);
    }
    if (SUCCEEDED(result)) {
        result = setValue(classKey + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    }
    return result;
}

// Remove an extension-point entry only when it still refers to this DLL's class.
void removeHandlerIfOurs(const wchar_t *handlerId, const GUID &clsid) {
    const auto handlerKey = std::wstring(kClassesKey) + kExtension + L"\\ShellEx\\" + handlerId;
    if (readValue(handlerKey, nullptr) == guidString(clsid)) {
        RegDeleteTreeW(HKEY_CURRENT_USER, handlerKey.c_str());
    }
}

} // namespace

HRESULT registerServer() {
    std::array<wchar_t, MAX_PATH> path{};
    const auto length = GetModuleFileNameW(moduleHandle(), path.data(), MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    const std::wstring modulePath = path.data();
    const auto shellExtensionKey = std::wstring(kClassesKey) + kExtension + L"\\ShellEx\\";

    auto result = registerClass(kThumbnailProviderClsid, kThumbnailProviderName, modulePath);
    if (SUCCEEDED(result)) {
        result = registerClass(kPreviewHandlerClsid, kPreviewHandlerName, modulePath);
    }
    if (SUCCEEDED(result)) {
        result = setValue(std::wstring(kClassesKey) + L"CLSID\\" + guidString(kPreviewHandlerClsid),
                          L"AppID",
                          kPreviewHostAppId);
    }
    if (SUCCEEDED(result)) {
        result = setValue(
            shellExtensionKey + kThumbnailHandlerId, nullptr, guidString(kThumbnailProviderClsid));
    }
    if (SUCCEEDED(result)) {
        result = setValue(
            shellExtensionKey + kPreviewHandlerId, nullptr, guidString(kPreviewHandlerClsid));
    }
    if (SUCCEEDED(result)) {
        result = setValue(
            kPreviewHandlersKey, guidString(kPreviewHandlerClsid).c_str(), kPreviewHandlerName);
    }
    if (SUCCEEDED(result)) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    return result;
}

HRESULT unregisterServer() {
    removeHandlerIfOurs(kThumbnailHandlerId, kThumbnailProviderClsid);
    removeHandlerIfOurs(kPreviewHandlerId, kPreviewHandlerClsid);
    for (const auto &clsid : {kThumbnailProviderClsid, kPreviewHandlerClsid}) {
        const auto classKey = std::wstring(kClassesKey) + L"CLSID\\" + guidString(clsid);
        RegDeleteTreeW(HKEY_CURRENT_USER, classKey.c_str());
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPreviewHandlersKey, 0, KEY_SET_VALUE, &key) ==
        ERROR_SUCCESS) {
        RegDeleteValueW(key, guidString(kPreviewHandlerClsid).c_str());
        RegCloseKey(key);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

} // namespace QuickPvrShell
