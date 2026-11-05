#pragma once

#include "Models/IVideoModel.h"

class VideoModel : public IVideoModel
{
public:
    VideoModel() = default;
    ~VideoModel() override = default;

    bool OpenFile(const std::wstring& filePath) override;
    void Play() override;
    void Pause() override;
    void Stop() override;

    void SetRepeat(bool enabled) override;
    bool IsRepeatEnabled() const override;

    PlaybackState GetState() const override;
    const std::wstring& GetFilePath() const override;
    bool HasMedia() const override;

private:
    std::wstring m_filePath;
    PlaybackState m_state = PlaybackState::Stopped;
    bool m_isRepeatEnabled = true;
};

