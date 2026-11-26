#pragma once

#include <windows.h>

#include <propsys.h>

#include "module.h"

namespace QuickPvrShell {

/**
 * Read-only Explorer properties of PowerVR and Sega Dreamcast PVR textures: dimensions, bit depth,
 * pixel format, and the number of textures in a pack.
 */
class PropertyHandler final :
    public IInitializeWithStream,
    public IPropertyStore,
    public IPropertyStoreCapabilities,
    ComObject {
public:
    ~PropertyHandler() override;

    IFACEMETHODIMP QueryInterface(REFIID iid, void **object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream *stream, DWORD mode) override;

    IFACEMETHODIMP GetCount(DWORD *count) override;
    IFACEMETHODIMP GetAt(DWORD index, PROPERTYKEY *key) override;
    IFACEMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT *value) override;
    IFACEMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT value) override;
    IFACEMETHODIMP Commit() override;

    IFACEMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override;

private:
    IPropertyStoreCache *cache_{};
};

} // namespace QuickPvrShell
