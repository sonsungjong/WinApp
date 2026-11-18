#pragma once

#include "framework.h"
#include <dwrite_3.h>
#include <wrl/client.h>

namespace FontHelper
{
    std::vector<std::wstring> GetBundledFontPaths();
    UINT GetWindowDpi(HWND hwnd);
    int ScaleDipToPx(int dip, UINT dpi);
    float PxToDip(int px, UINT dpi);
    HFONT CreateGdiFont(UINT dpi, int dipHeight = -14);
    class Resources
    {
    public:
        Resources();
        ~Resources();
        Resources(const Resources&) = delete;
        Resources& operator=(const Resources&) = delete;
        HFONT GetDialogFont() const { return m_font; }

    private:
        std::vector<std::wstring> m_loadedFonts;
        HFONT m_font = nullptr;
        bool m_ownsFont = false;
    };
    void CreateCustomFontCollection(
        IDWriteFactory* dwriteFactory,
        Microsoft::WRL::ComPtr<IDWriteFontCollection1>& outCollection,
        const wchar_t*& outKoreanFontName);
}
