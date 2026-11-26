#pragma once

#include <atomic>

#include <windows.h>

#include <unknwn.h>

namespace QuickPvrShell {

/** CLSID of the thumbnail provider: {3E28DA40-B072-4328-821D-F2A390548485}. */
constexpr CLSID kThumbnailProviderClsid = {
    0x3e28da40, 0xb072, 0x4328, {0x82, 0x1d, 0xf2, 0xa3, 0x90, 0x54, 0x84, 0x85}};
/** CLSID of the preview handler: {AC027A4E-FFCB-4B25-A7B1-82AA9438AD36}. */
constexpr CLSID kPreviewHandlerClsid = {
    0xac027a4e, 0xffcb, 0x4b25, {0xa7, 0xb1, 0x82, 0xaa, 0x94, 0x38, 0xad, 0x36}};
/** CLSID of the property handler: {4E50AC88-3201-4D83-94E6-EC3284EF90C6}. */
constexpr CLSID kPropertyHandlerClsid = {
    0x4e50ac88, 0x3201, 0x4d83, {0x94, 0xe6, 0xec, 0x32, 0x84, 0xef, 0x90, 0xc6}};
/**
 * Format ID of the QuickPVR properties: {1E525950-E124-46EB-A8CF-06D726640602}. Property 2 is
 * `QuickPVR.TextureCount`, described in QuickPVR.propdesc.
 */
constexpr GUID kPropertyFormatId = {
    0x1e525950, 0xe124, 0x46eb, {0xa8, 0xcf, 0x06, 0xd7, 0x26, 0x64, 0x06, 0x02}};
/** Property ID of `QuickPVR.TextureCount` in kPropertyFormatId. */
constexpr DWORD kTextureCountPropertyId = 2;

/** Handle of this DLL. */
HINSTANCE moduleHandle();
/** Record the handle of this DLL, from `DllMain`. */
void setModuleHandle(HINSTANCE handle);

/** Count a live object or server lock. The DLL is not unloaded while any count remains. */
void addModuleReference();
/** Release a count taken with addModuleReference(). */
void releaseModuleReference();
/** Whether any object or server lock is live. */
bool hasModuleReferences();

/**
 * Reference counting for a COM object that also prevents the DLL from unloading until the object
 * is deleted. Derived classes implement `QueryInterface` and call addReference() and release()
 * from `AddRef` and `Release`.
 */
class ComObject {
public:
    ComObject();
    virtual ~ComObject();

    ComObject(const ComObject &) = delete;
    ComObject &operator=(const ComObject &) = delete;

protected:
    /** Increment the reference count and return the new count. */
    ULONG addReference();
    /** Decrement the reference count, delete the object at zero, and return the new count. */
    ULONG release();

private:
    std::atomic<ULONG> referenceCount_{1};
};

} // namespace QuickPvrShell
