#pragma once

#include "Models/IVideoModel.h"

struct mpv_handle;

class VideoModel : public IVideoModel
{
public:
    VideoModel() = default;
    ~VideoModel() override;

    bool Initialize(HWND videoHwnd) override;
    bool OpenFile(const std::wstring& filePath) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void ProcessEvents() override;

    void SetRepeat(bool enabled) override;
    bool IsRepeatEnabled() const override;

    PlaybackState GetState() const override;
    const std::wstring& GetFilePath() const override;
    bool HasMedia() const override;

private:
    static std::string ToUtf8(const std::wstring& wideStr);

    mpv_handle* m_mpv = nullptr;
    std::wstring m_filePath;
    PlaybackState m_state = PlaybackState::Stopped;
    bool m_isRepeatEnabled = true;
};
