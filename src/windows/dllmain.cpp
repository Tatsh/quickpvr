#include <new>

#include <windows.h>

#include "classfactory.h"
#include "module.h"
#include "previewhandler.h"
#include "registration.h"
#include "thumbnailprovider.h"

using namespace QuickPvrShell;

namespace {

IUnknown *createThumbnailProvider() {
    return static_cast<IThumbnailProvider *>(new (std::nothrow) ThumbnailProvider);
}

IUnknown *createPreviewHandler() {
    return static_cast<IPreviewHandler *>(new (std::nothrow) PreviewHandler);
}

} // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        setModuleHandle(instance);
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID iid, void **object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = nullptr;
    ClassFactory::Creator creator = nullptr;
    if (IsEqualCLSID(clsid, kThumbnailProviderClsid)) {
        creator = createThumbnailProvider;
    } else if (IsEqualCLSID(clsid, kPreviewHandlerClsid)) {
        creator = createPreviewHandler;
    } else {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto *factory = new (std::nothrow) ClassFactory(creator);
    if (factory == nullptr) {
        return E_OUTOFMEMORY;
    }
    const auto result = factory->QueryInterface(iid, object);
    factory->Release();
    return result;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return hasModuleReferences() ? S_FALSE : S_OK;
}

extern "C" HRESULT __stdcall DllRegisterServer() {
    return registerServer();
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    return unregisterServer();
}
