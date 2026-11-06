#pragma once

#include "Models/IVideoModel.h"
#include <cstdint>

struct mpv_handle;

class VideoModel : public IVideoModel
{
public:
    VideoModel() = default;
    ~VideoModel() override;
    VideoModel(const VideoModel&) = delete;
    VideoModel& operator=(const VideoModel&) = delete;

    bool Initialize(HWND videoHwnd) override;
    bool OpenFile(const std::wstring& filePath) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void ProcessEvents() override;

    void SeekAbsolute(double seconds) override;
    void SeekRelative(double deltaSeconds) override;
    double GetPosition() const override;
    double GetDuration() const override;

    void SetVolume(int volume) override;
    int GetVolume() const override;
    void SetMuted(bool muted) override;
    bool IsMuted() const override;

    void SetSpeed(double speed) override;
    double GetSpeed() const override;

    void SetRepeat(bool enabled) override;
    bool IsRepeatEnabled() const override;

    void SetABLoopA() override;
    void SetABLoopB() override;
    void ClearABLoop() override;
    double GetABLoopA() const override;
    double GetABLoopB() const override;

    bool LoadSubtitleFile(const std::wstring& subFilePath) override;
    void SetSubtitleVisible(bool visible) override;
    bool IsSubtitleVisible() const override;
    void AdjustSubtitleDelay(double deltaSeconds) override;
    double GetSubtitleDelay() const override;
    void SetSubtitleScale(double scale) override;
    double GetSubtitleScale() const override;
    void AdjustSubtitlePos(int deltaPos) override;

    bool TakeScreenshot(const std::wstring& filePath) override;

    void SetOnPlaybackEnded(std::function<void()> handler) override;

    PlaybackState GetState() const override;
    const std::wstring& GetFilePath() const override;
    bool HasMedia() const override;

private:
    bool LoadFileInternal(const std::wstring& filePath);
    static std::string ToUtf8(const std::wstring& wideStr);

    mpv_handle* m_mpv = nullptr;
    std::int64_t m_playbackEntryId = -1;
    std::wstring m_filePath;
    PlaybackState m_state = PlaybackState::Stopped;

    bool m_isRepeatEnabled = false;
    int m_volume = 100;
    bool m_isMuted = false;
    double m_speed = 1.0;

    double m_abLoopA = -1.0;
    double m_abLoopB = -1.0;

    bool m_subtitleVisible = true;
    double m_subtitleDelay = 0.0;
    double m_subtitleScale = 1.0;
    int m_subtitlePos = 100;

    std::function<void()> m_onPlaybackEnded;
};
