// Loads the shell extension DLL through its class factory, the way Explorer does, and renders a
// thumbnail and a preview of the samples.
#include <cstdlib>
#include <iostream>
#include <string>

#include <windows.h>

#include <shlwapi.h>
#include <shobjidl.h>
#include <thumbcache.h>

namespace {

constexpr CLSID kThumbnailProviderClsid = {
    0x3e28da40, 0xb072, 0x4328, {0x82, 0x1d, 0xf2, 0xa3, 0x90, 0x54, 0x84, 0x85}};
constexpr CLSID kPreviewHandlerClsid = {
    0xac027a4e, 0xffcb, 0x4b25, {0xa7, 0xb1, 0x82, 0xaa, 0x94, 0x38, 0xad, 0x36}};
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

    std::cout << (failures == 0 ? "all passed\n" : "failures\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
