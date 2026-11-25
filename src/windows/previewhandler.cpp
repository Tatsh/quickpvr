#include "previewhandler.h"

#include <algorithm>
#include <utility>

#include <shlwapi.h>

#include "shellimage.h"

namespace QuickPvrShell {

namespace {

constexpr wchar_t kWindowClassName[] = L"QuickPVRPreviewWindow";
constexpr int kMargin = 8;
constexpr int kTextHeight = 24;
constexpr std::size_t kChannelCount = 4;
constexpr unsigned kOpaque = 255;

ATOM registerWindowClass(WNDPROC procedure) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = procedure;
    windowClass.hInstance = moduleHandle();
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;
    const auto atom = RegisterClassExW(&windowClass);
    // The class remains registered for a later handler in the same process.
    if (atom == 0 && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        return 1;
    }
    return atom;
}

// Blend onto the background first because GDI does not blend alpha with StretchDIBits.
std::vector<std::uint8_t> blendOnto(const Pvr::Image &image, COLORREF background) {
    const unsigned backgroundRed = GetRValue(background);
    const unsigned backgroundGreen = GetGValue(background);
    const unsigned backgroundBlue = GetBValue(background);
    std::vector<std::uint8_t> out(image.rgba.size());
    for (std::size_t i = 0; i < image.rgba.size(); i += kChannelCount) {
        const unsigned alpha = image.rgba[i + 3];
        const auto blend = [alpha](unsigned value, unsigned under) {
            return static_cast<std::uint8_t>((value * alpha + under * (kOpaque - alpha)) / kOpaque);
        };
        out[i] = blend(image.rgba[i + 2], backgroundBlue);
        out[i + 1] = blend(image.rgba[i + 1], backgroundGreen);
        out[i + 2] = blend(image.rgba[i], backgroundRed);
        out[i + 3] = 0;
    }
    return out;
}

} // namespace

PreviewHandler::~PreviewHandler() {
    Unload();
    releaseSite();
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
}

IFACEMETHODIMP PreviewHandler::QueryInterface(REFIID iid, void **object) {
    static const QITAB interfaces[] = {
        QITABENT(PreviewHandler, IInitializeWithStream),
        QITABENT(PreviewHandler, IObjectWithSite),
        QITABENT(PreviewHandler, IOleWindow),
        QITABENT(PreviewHandler, IPreviewHandler),
        QITABENT(PreviewHandler, IPreviewHandlerVisuals),
        {nullptr, 0},
    };
    return QISearch(this, interfaces, iid, object);
}

IFACEMETHODIMP_(ULONG) PreviewHandler::AddRef() {
    return addReference();
}

IFACEMETHODIMP_(ULONG) PreviewHandler::Release() {
    return release();
}

IFACEMETHODIMP PreviewHandler::Initialize(IStream *stream, DWORD) {
    auto data = readStream(stream);
    if (!data) {
        return data.error();
    }
    data_ = std::move(*data);
    image_.reset();
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::SetSite(IUnknown *site) {
    releaseSite();
    if (site != nullptr) {
        site_ = site;
        site_->AddRef();
        // The frame translates keyboard accelerators for the preview pane.
        site_->QueryInterface(IID_PPV_ARGS(&frame_));
    }
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::GetSite(REFIID iid, void **site) {
    if (site == nullptr) {
        return E_POINTER;
    }
    *site = nullptr;
    return site_ != nullptr ? site_->QueryInterface(iid, site) : E_FAIL;
}

IFACEMETHODIMP PreviewHandler::GetWindow(HWND *window) {
    if (window == nullptr) {
        return E_INVALIDARG;
    }
    *window = parent_;
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::ContextSensitiveHelp(BOOL) {
    return E_NOTIMPL;
}

IFACEMETHODIMP PreviewHandler::SetWindow(HWND parent, const RECT *bounds) {
    if (parent == nullptr || bounds == nullptr) {
        return E_INVALIDARG;
    }
    parent_ = parent;
    bounds_ = *bounds;
    if (window_ != nullptr) {
        SetParent(window_, parent_);
        SetWindowPos(window_,
                     nullptr,
                     bounds_.left,
                     bounds_.top,
                     bounds_.right - bounds_.left,
                     bounds_.bottom - bounds_.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::SetRect(const RECT *bounds) {
    if (bounds == nullptr) {
        return E_INVALIDARG;
    }
    bounds_ = *bounds;
    if (window_ != nullptr) {
        SetWindowPos(window_,
                     nullptr,
                     bounds_.left,
                     bounds_.top,
                     bounds_.right - bounds_.left,
                     bounds_.bottom - bounds_.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(window_, nullptr, TRUE);
    }
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::DoPreview() {
    if (parent_ == nullptr || data_.empty()) {
        return E_FAIL;
    }
    if (!image_) {
        auto texture = decodeTexture(data_);
        if (!texture) {
            return texture.error();
        }
        image_ = std::move(texture->image);
        summary_ = std::move(texture->summary);
    }
    if (window_ == nullptr) {
        if (registerWindowClass(windowProcedure) == 0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        window_ = CreateWindowExW(0,
                                  kWindowClassName,
                                  nullptr,
                                  WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
                                  bounds_.left,
                                  bounds_.top,
                                  bounds_.right - bounds_.left,
                                  bounds_.bottom - bounds_.top,
                                  parent_,
                                  nullptr,
                                  moduleHandle(),
                                  nullptr);
        if (window_ == nullptr) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    InvalidateRect(window_, nullptr, TRUE);
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::Unload() {
    if (window_ != nullptr) {
        SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
        DestroyWindow(window_);
        window_ = nullptr;
    }
    image_.reset();
    data_.clear();
    summary_.clear();
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::SetFocus() {
    return S_FALSE;
}

IFACEMETHODIMP PreviewHandler::QueryFocus(HWND *window) {
    if (window == nullptr) {
        return E_INVALIDARG;
    }
    *window = GetFocus();
    return *window != nullptr ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

IFACEMETHODIMP PreviewHandler::TranslateAccelerator(MSG *message) {
    return frame_ != nullptr ? frame_->TranslateAccelerator(message) : S_FALSE;
}

IFACEMETHODIMP PreviewHandler::SetBackgroundColor(COLORREF color) {
    backgroundColor_ = color;
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::SetFont(const LOGFONTW *font) {
    if (font == nullptr) {
        return E_INVALIDARG;
    }
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
    font_ = CreateFontIndirectW(font);
    return S_OK;
}

IFACEMETHODIMP PreviewHandler::SetTextColor(COLORREF color) {
    textColor_ = color;
    return S_OK;
}

LRESULT CALLBACK PreviewHandler::windowProcedure(HWND window,
                                                 UINT message,
                                                 WPARAM wParam,
                                                 LPARAM lParam) {
    const auto *handler =
        reinterpret_cast<const PreviewHandler *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_ERASEBKGND:
        // WM_PAINT fills the whole window.
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paintStruct;
        HDC context = BeginPaint(window, &paintStruct);
        if (handler != nullptr) {
            RECT client;
            GetClientRect(window, &client);
            handler->paint(context, client);
        }
        EndPaint(window, &paintStruct);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

void PreviewHandler::paint(HDC context, const RECT &client) const {
    HBRUSH background = CreateSolidBrush(backgroundColor_);
    FillRect(context, &client, background);
    DeleteObject(background);
    if (!image_) {
        return;
    }
    const int availableWidth = client.right - client.left - 2 * kMargin;
    const int availableHeight = client.bottom - client.top - 3 * kMargin - kTextHeight;
    if (availableWidth <= 0 || availableHeight <= 0) {
        return;
    }
    const auto &image = *image_;
    const double scale = std::min(static_cast<double>(availableWidth) / image.width,
                                  static_cast<double>(availableHeight) / image.height);
    const int width = std::max(1, static_cast<int>(image.width * scale));
    const int height = std::max(1, static_cast<int>(image.height * scale));
    const int left = client.left + (client.right - client.left - width) / 2;
    const int top = client.top + kMargin + (availableHeight - height) / 2;

    const auto pixels = blendOnto(image, backgroundColor_);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = static_cast<LONG>(image.width);
    info.bmiHeader.biHeight = -static_cast<LONG>(image.height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    // Enlarging repeats texels so they remain sharp; shrinking averages them.
    SetStretchBltMode(context, scale > 1 ? COLORONCOLOR : HALFTONE);
    SetBrushOrgEx(context, 0, 0, nullptr);
    StretchDIBits(context,
                  left,
                  top,
                  width,
                  height,
                  0,
                  0,
                  static_cast<int>(image.width),
                  static_cast<int>(image.height),
                  pixels.data(),
                  &info,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    RECT textBounds{client.left + kMargin,
                    client.bottom - kMargin - kTextHeight,
                    client.right - kMargin,
                    client.bottom - kMargin};
    const auto previousFont = font_ != nullptr ? SelectObject(context, font_) : nullptr;
    ::SetTextColor(context, textColor_);
    SetBkMode(context, TRANSPARENT);
    DrawTextW(context,
              summary_.c_str(),
              static_cast<int>(summary_.size()),
              &textBounds,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (previousFont != nullptr) {
        SelectObject(context, previousFont);
    }
}

void PreviewHandler::releaseSite() {
    if (frame_ != nullptr) {
        frame_->Release();
        frame_ = nullptr;
    }
    if (site_ != nullptr) {
        site_->Release();
        site_ = nullptr;
    }
}

} // namespace QuickPvrShell
