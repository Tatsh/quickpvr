#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include <ocidl.h>
#include <propsys.h>
#include <shobjidl.h>

#include "module.h"
#include "pvr/pvr.h"

namespace QuickPvrShell {

/** Explorer preview pane for PowerVR and Sega Dreamcast PVR textures and texture packs. */
class PreviewHandler final :
    public IInitializeWithStream,
    public IObjectWithSite,
    public IOleWindow,
    public IPreviewHandler,
    public IPreviewHandlerVisuals,
    ComObject {
public:
    ~PreviewHandler() override;

    IFACEMETHODIMP QueryInterface(REFIID iid, void **object) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream *stream, DWORD mode) override;

    IFACEMETHODIMP SetSite(IUnknown *site) override;
    IFACEMETHODIMP GetSite(REFIID iid, void **site) override;

    IFACEMETHODIMP GetWindow(HWND *window) override;
    IFACEMETHODIMP ContextSensitiveHelp(BOOL enterMode) override;

    IFACEMETHODIMP SetWindow(HWND parent, const RECT *bounds) override;
    IFACEMETHODIMP SetRect(const RECT *bounds) override;
    IFACEMETHODIMP DoPreview() override;
    IFACEMETHODIMP Unload() override;
    IFACEMETHODIMP SetFocus() override;
    IFACEMETHODIMP QueryFocus(HWND *window) override;
    IFACEMETHODIMP TranslateAccelerator(MSG *message) override;

    IFACEMETHODIMP SetBackgroundColor(COLORREF color) override;
    IFACEMETHODIMP SetFont(const LOGFONTW *font) override;
    IFACEMETHODIMP SetTextColor(COLORREF color) override;

private:
    static LRESULT CALLBACK windowProcedure(HWND window,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam);
    void paint(HDC context, const RECT &client) const;
    void releaseSite();

    std::vector<std::uint8_t> data_;
    std::optional<Pvr::Image> image_;
    std::wstring summary_;
    IUnknown *site_{};
    IPreviewHandlerFrame *frame_{};
    HWND parent_{};
    HWND window_{};
    RECT bounds_{};
    HFONT font_{};
    COLORREF backgroundColor_{GetSysColor(COLOR_WINDOW)};
    COLORREF textColor_{GetSysColor(COLOR_WINDOWTEXT)};
};

} // namespace QuickPvrShell
