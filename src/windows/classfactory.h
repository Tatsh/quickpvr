#pragma once

#include <windows.h>

#include <unknwn.h>

#include "module.h"

namespace QuickPvrShell {

/** Creates one kind of COM object for `DllGetClassObject`. */
class ClassFactory final : public IClassFactory, ComObject {
public:
    /** Function creating a new object with a reference count of one. */
    using Creator = IUnknown *(*)();

    /**
     * Create a factory.
     *
     * @param creator Function creating the objects.
     */
    explicit ClassFactory(Creator creator);

    IFACEMETHODIMP QueryInterface(REFIID iid, void **object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP CreateInstance(IUnknown *outer, REFIID iid, void **object) override;
    IFACEMETHODIMP LockServer(BOOL lock) override;

private:
    Creator creator_;
};

} // namespace QuickPvrShell
