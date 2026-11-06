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

    Resources::Resources()
    {
        for (const auto& path : GetBundledFontPaths())
        {
            if (AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) > 0)
                m_loadedFonts.push_back(path);
        }
        m_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"NanumSquare");
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

