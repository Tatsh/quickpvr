#include "classfactory.h"

#include <shlwapi.h>

namespace QuickPvrShell {

ClassFactory::ClassFactory(Creator creator) : creator_(creator) {
}

IFACEMETHODIMP ClassFactory::QueryInterface(REFIID iid, void **object) {
    static const QITAB interfaces[] = {
        QITABENT(ClassFactory, IClassFactory),
        {nullptr, 0},
    };
    return QISearch(this, interfaces, iid, object);
}

IFACEMETHODIMP_(ULONG) ClassFactory::AddRef() {
    return addReference();
}

IFACEMETHODIMP_(ULONG) ClassFactory::Release() {
    return release();
}

IFACEMETHODIMP ClassFactory::CreateInstance(IUnknown *outer, REFIID iid, void **object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = nullptr;
    if (outer != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }
    IUnknown *instance = creator_();
    if (instance == nullptr) {
        return E_OUTOFMEMORY;
    }
    const auto result = instance->QueryInterface(iid, object);
    instance->Release();
    return result;
}

IFACEMETHODIMP ClassFactory::LockServer(BOOL lock) {
    if (lock) {
        addModuleReference();
    } else {
        releaseModuleReference();
    }
    return S_OK;
}

} // namespace QuickPvrShell
