#include "Views/FontHelper.h"
#include <filesystem>

namespace FontHelper
{
    std::vector<std::wstring> GetBundledFontPaths()
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();

        std::vector<std::filesystem::path> baseDirs = {
            exeDir / L"ThirdParty" / L"fonts",
            exeDir.parent_path() / L"ThirdParty" / L"fonts"
        };

        WCHAR localAppData[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0)
        {
            baseDirs.push_back(std::filesystem::path(localAppData) / L"Microsoft" / L"Windows" / L"Fonts");
        }

        std::vector<std::wstring> found;
        for (const wchar_t* fileName : { L"NanumSquareR.ttf", L"NanumSquareB.ttf" })
        {
            for (const auto& dir : baseDirs)
            {
                std::filesystem::path candidate = dir / fileName;
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec))
                {
                    found.push_back(candidate.wstring());
                    break;
                }
            }
        }
        return found;
    }

    UINT GetWindowDpi(HWND hwnd)
    {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        using GetDpiForSystemFn = UINT(WINAPI*)();
        static const auto getDpiForWindow = []() -> GetDpiForWindowFn {
            if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
                return reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
            return nullptr;
        }();
        static const auto getDpiForSystem = []() -> GetDpiForSystemFn {
            if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
                return reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem"));
            return nullptr;
        }();

        if (hwnd && getDpiForWindow)
        {
            const UINT dpi = getDpiForWindow(hwnd);
            if (dpi >= 96) return dpi;
        }
        if (getDpiForSystem)
        {
            const UINT dpi = getDpiForSystem();
            if (dpi >= 96) return dpi;
        }
        HDC hdc = GetDC(nullptr);
        const UINT fallback = hdc ? static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX)) : 96u;
        if (hdc) ReleaseDC(nullptr, hdc);
        return fallback >= 96 ? fallback : 96u;
    }

    int ScaleDipToPx(int dip, UINT dpi)
    {
        return MulDiv(dip, static_cast<int>(dpi ? dpi : 96u), 96);
    }

    float PxToDip(int px, UINT dpi)
    {
        const UINT effectiveDpi = dpi ? dpi : 96u;
        return static_cast<float>(px) * 96.0f / static_cast<float>(effectiveDpi);
    }

    HFONT CreateGdiFont(UINT dpi, int dipHeight)
    {
        const int scaledHeight = MulDiv(dipHeight, static_cast<int>(dpi ? dpi : 96u), 96);
        // Use hinted Malgun Gothic for GDI Win32 child controls; unhinted NanumSquare tears under GDI ClearType.
        HFONT font = CreateFontW(scaledHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
        if (!font)
        {
            font = CreateFontW(scaledHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        }
        return font;
    }

    Resources::Resources()
    {
        for (const auto& path : GetBundledFontPaths())
        {
            if (AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) > 0)
                m_loadedFonts.push_back(path);
        }
        m_font = CreateGdiFont(GetWindowDpi(nullptr), -13);
        m_ownsFont = m_font != nullptr;
        if (!m_font) m_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    Resources::~Resources()
    {
        if (m_ownsFont) DeleteObject(m_font);
        for (const auto& path : m_loadedFonts)
            RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
    }

    void CreateCustomFontCollection(
        IDWriteFactory* dwriteFactory,
        Microsoft::WRL::ComPtr<IDWriteFontCollection1>& outCollection,
        const wchar_t*& outKoreanFontName)
    {
        std::vector<std::wstring> fontPaths = GetBundledFontPaths();

        Microsoft::WRL::ComPtr<IDWriteFactory5> dwriteFactory5;
        if (dwriteFactory && !fontPaths.empty() &&
            SUCCEEDED(dwriteFactory->QueryInterface(__uuidof(IDWriteFactory5), reinterpret_cast<void**>(dwriteFactory5.GetAddressOf()))))
        {
            Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
            if (SUCCEEDED(dwriteFactory5->CreateFontSetBuilder(&fontSetBuilder)))
            {
                for (const auto& path : fontPaths)
                {
                    Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile;
                    if (SUCCEEDED(dwriteFactory5->CreateFontFileReference(path.c_str(), nullptr, &fontFile)))
                    {
                        fontSetBuilder->AddFontFile(fontFile.Get());
                    }
                }
                Microsoft::WRL::ComPtr<IDWriteFontSet> fontSet;
                if (SUCCEEDED(fontSetBuilder->CreateFontSet(&fontSet)))
                {
                    dwriteFactory5->CreateFontCollectionFromFontSet(fontSet.Get(), &outCollection);
                }
            }
        }

        outKoreanFontName = outCollection ? L"NanumSquare" : L"Malgun Gothic";

        if (!outCollection && dwriteFactory)
        {
            Microsoft::WRL::ComPtr<IDWriteFontCollection> sysCollection;
            if (SUCCEEDED(dwriteFactory->GetSystemFontCollection(&sysCollection)) && sysCollection)
            {
                UINT32 idx = 0;
                BOOL exists = FALSE;
                if (SUCCEEDED(sysCollection->FindFamilyName(L"NanumSquare", &idx, &exists)) && exists)
                {
                    outKoreanFontName = L"NanumSquare";
                }
            }
        }
    }
}

