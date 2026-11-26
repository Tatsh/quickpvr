// Loads the shell extension DLL through its class factory, the way Explorer does, and renders a
// thumbnail and a preview of the samples.
#include <cstdlib>
#include <iostream>
#include <string>

#include <windows.h>

// initguid.h makes propkey.h define the property keys instead of only declaring them.
#include <initguid.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <thumbcache.h>

namespace {

constexpr CLSID kThumbnailProviderClsid = {
    0x3e28da40, 0xb072, 0x4328, {0x82, 0x1d, 0xf2, 0xa3, 0x90, 0x54, 0x84, 0x85}};
constexpr CLSID kPreviewHandlerClsid = {
    0xac027a4e, 0xffcb, 0x4b25, {0xa7, 0xb1, 0x82, 0xaa, 0x94, 0x38, 0xad, 0x36}};
constexpr CLSID kPropertyHandlerClsid = {
    0x4e50ac88, 0x3201, 0x4d83, {0x94, 0xe6, 0xec, 0x32, 0x84, 0xef, 0x90, 0xc6}};
constexpr PROPERTYKEY kTextureCountKey = {
    {0x1e525950, 0xe124, 0x46eb, {0xa8, 0xcf, 0x06, 0xd7, 0x26, 0x64, 0x06, 0x02}}, 2};
constexpr UINT kThumbnailSize = 256;

using GetClassObject = HRESULT(__stdcall *)(REFCLSID, REFIID, void **);

int failures = 0;

void check(bool condition, const char *description) {
    std::cout << (condition ? "ok   " : "FAIL ") << description << '\n';
    if (!condition) {
        ++failures;
    }
}

std::wstring sample(const wchar_t *name) {
    return std::wstring(PVR_SAMPLES_DIR_WIDE L"/") + name;
}

template <typename Interface>
Interface *createObject(GetClassObject getClassObject, const CLSID &clsid) {
    IClassFactory *factory = nullptr;
    if (FAILED(getClassObject(clsid, IID_PPV_ARGS(&factory)))) {
        return nullptr;
    }
    Interface *object = nullptr;
    factory->CreateInstance(nullptr, IID_PPV_ARGS(&object));
    factory->Release();
    return object;
}

bool initializeWithFile(IUnknown *object, const std::wstring &path) {
    IInitializeWithStream *initializer = nullptr;
    if (FAILED(object->QueryInterface(IID_PPV_ARGS(&initializer)))) {
        return false;
    }
    IStream *stream = nullptr;
    auto result = SHCreateStreamOnFileEx(path.c_str(),
                                         STGM_READ | STGM_SHARE_DENY_NONE,
                                         FILE_ATTRIBUTE_NORMAL,
                                         FALSE,
                                         nullptr,
                                         &stream);
    if (SUCCEEDED(result)) {
        result = initializer->Initialize(stream, STGM_READ);
        stream->Release();
    }
    initializer->Release();
    return SUCCEEDED(result);
}

SIZE thumbnailSize(GetClassObject getClassObject, const std::wstring &path, HRESULT &result) {
    SIZE size{};
    result = E_FAIL;
    auto *provider = createObject<IThumbnailProvider>(getClassObject, kThumbnailProviderClsid);
    if (provider == nullptr || !initializeWithFile(provider, path)) {
        if (provider != nullptr) {
            provider->Release();
        }
        return size;
    }
    HBITMAP bitmap = nullptr;
    WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
    result = provider->GetThumbnail(kThumbnailSize, &bitmap, &alphaType);
    if (SUCCEEDED(result) && bitmap != nullptr) {
        BITMAP info{};
        GetObjectW(bitmap, sizeof(info), &info);
        size = {info.bmWidth, std::abs(info.bmHeight)};
        DeleteObject(bitmap);
    }
    provider->Release();
    return size;
}

// An empty PROPVARIANT means the handler does not provide the property.
PROPVARIANT propertyValue(IPropertyStore *store, REFPROPERTYKEY key) {
    PROPVARIANT value;
    PropVariantInit(&value);
    store->GetValue(key, &value);
    return value;
}

ULONG numberProperty(IPropertyStore *store, REFPROPERTYKEY key) {
    auto value = propertyValue(store, key);
    ULONG number = 0;
    PropVariantToUInt32(value, &number);
    PropVariantClear(&value);
    return number;
}

std::wstring textProperty(IPropertyStore *store, REFPROPERTYKEY key) {
    auto value = propertyValue(store, key);
    std::wstring text = value.vt == VT_LPWSTR ? value.pwszVal : L"";
    PropVariantClear(&value);
    return text;
}

IPropertyStore *propertyStore(GetClassObject getClassObject, const std::wstring &path) {
    auto *store = createObject<IPropertyStore>(getClassObject, kPropertyHandlerClsid);
    if (store != nullptr && !initializeWithFile(store, path)) {
        store->Release();
        return nullptr;
    }
    return store;
}

} // namespace

int wmain(int argc, wchar_t *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: test_windows_shell QuickPVRShell.dll\n";
        return EXIT_FAILURE;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HMODULE module = LoadLibraryW(argv[1]);
    check(module != nullptr, "the DLL loads");
    if (module == nullptr) {
        return EXIT_FAILURE;
    }
    const auto getClassObject =
        reinterpret_cast<GetClassObject>(GetProcAddress(module, "DllGetClassObject"));
    check(getClassObject != nullptr, "DllGetClassObject is exported");
    check(GetProcAddress(module, "DllRegisterServer") != nullptr, "DllRegisterServer is exported");
    check(GetProcAddress(module, "DllInstall") != nullptr, "DllInstall is exported");
    if (getClassObject == nullptr) {
        return EXIT_FAILURE;
    }

    HRESULT result;
    const auto banner = thumbnailSize(getClassObject, sample(L"brighterday.pvr"), result);
    check(SUCCEEDED(result) && banner.cx == 256 && banner.cy == 64,
          "a PVR v2 banner thumbnail is 256x64");
    const auto logo = thumbnailSize(getClassObject, sample(L"dreamcast/logo_pal8.pvr"), result);
    check(SUCCEEDED(result) && logo.cx == 256 && logo.cy == 256,
          "a small texture is enlarged to the thumbnail size");
    thumbnailSize(getClassObject, sample(L"../CMakeLists.txt"), result);
    check(FAILED(result), "a file that is not a texture fails");

    auto *handler = createObject<IPreviewHandler>(getClassObject, kPreviewHandlerClsid);
    check(handler != nullptr, "the preview handler is created");
    if (handler != nullptr) {
        check(initializeWithFile(handler, sample(L"dreamcast/PACK_T.PVR")),
              "the preview handler reads a pack");
        HWND parent = CreateWindowExW(0,
                                      L"STATIC",
                                      L"",
                                      WS_OVERLAPPEDWINDOW,
                                      0,
                                      0,
                                      640,
                                      480,
                                      nullptr,
                                      nullptr,
                                      GetModuleHandleW(nullptr),
                                      nullptr);
        RECT bounds{0, 0, 640, 480};
        check(SUCCEEDED(handler->SetWindow(parent, &bounds)), "the preview window is attached");
        check(SUCCEEDED(handler->DoPreview()), "the preview is shown");
        check(SUCCEEDED(handler->Unload()), "the preview is unloaded");
        handler->Release();
        DestroyWindow(parent);
    }

    auto *texture = propertyStore(getClassObject, sample(L"dreamcast/logo_argb4444_twiddled.pvr"));
    check(texture != nullptr, "the property handler reads a texture");
    if (texture != nullptr) {
        check(numberProperty(texture, PKEY_Image_HorizontalSize) == 256 &&
                  numberProperty(texture, PKEY_Image_VerticalSize) == 256,
              "the width and height are 256");
        check(textProperty(texture, PKEY_Image_Dimensions) == L"256 x 256",
              "the dimensions are 256 x 256");
        check(numberProperty(texture, PKEY_Image_BitDepth) == 16, "the bit depth is 16");
        check(textProperty(texture, PKEY_Image_CompressionText)
                  .starts_with(L"Dreamcast PVR, ARGB4444, twiddled"),
              "the format includes the container and the pixel format");
        check(numberProperty(texture, kTextureCountKey) == 0,
              "a single texture does not have a texture count");
        PROPVARIANT value;
        InitPropVariantFromUInt32(1, &value);
        check(texture->SetValue(PKEY_Image_BitDepth, value) == STG_E_ACCESSDENIED,
              "the properties are read-only");
        texture->Release();
    }

    auto *pack = propertyStore(getClassObject, sample(L"dreamcast/PACK_T.PVR"));
    check(pack != nullptr, "the property handler reads a pack");
    if (pack != nullptr) {
        check(numberProperty(pack, kTextureCountKey) == 2, "the pack has 2 textures");
        check(numberProperty(pack, PKEY_Image_HorizontalSize) == 0,
              "a pack does not have a single width");
        check(textProperty(pack, PKEY_Image_CompressionText).find(L"256x64") != std::wstring::npos,
              "the format lists each texture of the pack");
        pack->Release();
    }

    auto *notTexture = propertyStore(getClassObject, sample(L"../CMakeLists.txt"));
    check(notTexture != nullptr, "the property handler accepts a file that is not a texture");
    if (notTexture != nullptr) {
        DWORD count = 1;
        check(SUCCEEDED(notTexture->GetCount(&count)) && count == 0,
              "a file that is not a texture does not have properties");
        notTexture->Release();
    }

    std::cout << (failures == 0 ? "all passed\n" : "failures\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
