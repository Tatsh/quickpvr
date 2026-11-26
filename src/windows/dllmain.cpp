#include <new>

#include <windows.h>

#include <shlwapi.h>

#include "classfactory.h"
#include "module.h"
#include "previewhandler.h"
#include "propertyhandler.h"
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

IUnknown *createPropertyHandler() {
    return static_cast<IPropertyStore *>(new (std::nothrow) PropertyHandler);
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
    } else if (IsEqualCLSID(clsid, kPropertyHandlerClsid)) {
        creator = createPropertyHandler;
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
    return registerServer(Scope::User);
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
    return unregisterServer(Scope::User);
}

// regsvr32 /n /i:machine registers for every user, and /n /i:user (or no argument) for the current
// user. Adding /u removes the registration.
extern "C" HRESULT __stdcall DllInstall(BOOL install, LPCWSTR commandLine) {
    const auto scope = commandLine != nullptr && StrCmpIW(commandLine, L"machine") == 0 ?
                           Scope::Machine :
                           Scope::User;
    return install ? registerServer(scope) : unregisterServer(scope);
}
