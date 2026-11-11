#include "Models/AppInfo.h"
#include "Resource.h"
#include "Version.h"
#include <cJSON.h>
#include <memory>
#include <stdexcept>

namespace
{
    std::string ResourceText(HINSTANCE instance, int id)
    {
        const auto resource = FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
        const auto loaded = resource ? LoadResource(instance, resource) : nullptr;
        const auto* data = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
        if (!data)
            throw std::runtime_error("App information resource is missing");
        return std::string(data, SizeofResource(instance, resource));
    }

    std::wstring WideText(const std::string& text)
    {
        if (text.empty())
            return {};
        const int size =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (!size)
            throw std::runtime_error("App information must use UTF-8");
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(),
                            size);
        std::wstring normalized;
        normalized.reserve(result.size());
        // Native multiline edit controls use CRLF for line breaks.
        for (std::size_t index = 0; index < result.size(); ++index)
        {
            if (result[index] == L'\n' && (index == 0 || result[index - 1] != L'\r'))
                normalized += L'\r';
            normalized += result[index];
        }
        return normalized;
    }
} // namespace

AppInfo AppInfo::Load(HINSTANCE instance)
{
    const auto source = ResourceText(instance, IDR_APP_INFO);
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_ParseWithLength(source.data(), source.size()),
                                                         &cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get()))
        throw std::runtime_error("Invalid AppInfo.json");
    auto field = [&](const char* name) {
        const auto* value = cJSON_GetObjectItemCaseSensitive(root.get(), name);
        if (!cJSON_IsString(value))
            throw std::runtime_error("App information field is missing");
        return WideText(value->valuestring);
    };
    AppInfo info;
    info.developer = field("developer");
    info.email = field("email");
    info.youtubeUrl = field("youtubeUrl");
    info.displayText = L"NextVideoPlayer " + WideText(NVP_VERSION_STRING) + L"\r\n" + field("description") +
                       L"\r\n\r\n개발자  " + info.developer + L"\r\n문의  " + info.email + L"\r\nYouTube  " +
                       (info.youtubeUrl.empty() ? L"채널 주소 등록 예정" : info.youtubeUrl) + L"\r\n\r\n사용 안내\r\n" +
                       field("usage");
    auto section = [&](const wchar_t* title, const char* name) {
        info.displayText += std::wstring(L"\r\n\r\n") + title + L"\r\n";
        const auto* values = cJSON_GetObjectItemCaseSensitive(root.get(), name);
        if (!cJSON_IsArray(values))
            throw std::runtime_error("App information section is missing");
        for (const auto* value = values->child; value; value = value->next)
        {
            if (!cJSON_IsString(value))
                throw std::runtime_error("Invalid app information entry");
            info.displayText += WideText(value->valuestring) + L"\r\n";
        }
    };
    section(L"최소 사양 · 사용 안내안", "minimumRequirements");
    section(L"권장 사양 · 사용 안내안", "recommendedRequirements");
    info.displayText += L"\r\n" + field("requirementsNote");
    section(L"외부 구성요소 및 라이선스", "thirdPartyNotices");
    info.displayText += L"\r\n" + field("licenseNote");
    return info;
}

std::wstring AppInfo::LicenseText(HINSTANCE instance)
{
    std::wstring text = L"NextVideoPlayer 외부 구성요소 라이선스\r\n\r\n"
                        L"추가 런타임 고지와 출처 정보는 설치 폴더의 licenses 폴더에서도 확인할 수 있습니다.\r\n\r\n";
    for (const auto id : {IDR_OSS_LICENSE, IDR_CJSON_LICENSE, IDR_LWS_LICENSE, IDR_LWS_NOTICE, IDR_FONT_LICENSE,
                         IDR_FFMPEG_LGPL_LICENSE, IDR_GPL3_LICENSE})
    {
        text += L"\r\n============================================================\r\n\r\n";
        text += WideText(ResourceText(instance, id));
    }
    return text;
}
