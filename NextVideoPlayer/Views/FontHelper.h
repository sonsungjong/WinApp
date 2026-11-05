#pragma once

#include "framework.h"
#include <dwrite_3.h>
#include <wrl/client.h>

namespace FontHelper
{
    std::vector<std::wstring> GetBundledFontPaths();
    void EnsureBundledFontsLoaded();
    HFONT GetNanumDialogFont();
    void CreateCustomFontCollection(
        IDWriteFactory* dwriteFactory,
        Microsoft::WRL::ComPtr<IDWriteFontCollection1>& outCollection,
        const wchar_t*& outKoreanFontName);
}

