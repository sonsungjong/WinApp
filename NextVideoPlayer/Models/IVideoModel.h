#pragma once

#include "framework.h"
#include <cstdint>

enum class PlaybackState
{
    Stopped,
    Playing,
    Paused
};

class IVideoModel
{
public:
    virtual ~IVideoModel() = default;

    virtual bool Initialize(HWND videoHwnd) = 0;
    virtual bool OpenFile(const std::wstring& filePath) = 0;
    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void Stop() = 0;
    virtual void ProcessEvents() = 0;

    virtual void SeekAbsolute(double seconds) = 0;
    virtual void SeekRelative(double deltaSeconds) = 0;
    virtual double GetPosition() const = 0;
    virtual double GetDuration() const = 0;

    virtual void SetVolume(int volume) = 0;
    virtual int GetVolume() const = 0;
    virtual void SetMuted(bool muted) = 0;
    virtual bool IsMuted() const = 0;

    virtual void SetSpeed(double speed) = 0;
    virtual double GetSpeed() const = 0;

    virtual void SetRepeat(bool enabled) = 0;
    virtual bool IsRepeatEnabled() const = 0;

    virtual void SetABLoopA() = 0;
    virtual void SetABLoopB() = 0;
    virtual void ClearABLoop() = 0;
    virtual double GetABLoopA() const = 0;
    virtual double GetABLoopB() const = 0;

    virtual bool LoadSubtitleFile(const std::wstring& subFilePath) = 0;
    virtual void SetSubtitleVisible(bool visible) = 0;
    virtual bool IsSubtitleVisible() const = 0;
    virtual void AdjustSubtitleDelay(double deltaSeconds) = 0;
    virtual double GetSubtitleDelay() const = 0;
    virtual void SetSubtitleScale(double scale) = 0;
    virtual double GetSubtitleScale() const = 0;
    virtual void AdjustSubtitlePos(int deltaPos) = 0;

    virtual bool TakeScreenshot(const std::wstring& filePath) = 0;

    virtual void SetOnPlaybackEnded(std::function<void()> handler) = 0;

    virtual PlaybackState GetState() const = 0;
    virtual const std::wstring& GetFilePath() const = 0;
    virtual bool HasMedia() const = 0;
    virtual bool IsMediaReady() const { return HasMedia(); }
    virtual bool IsLoading() const { return false; }
    virtual std::string GetMediaError() const { return {}; }
    virtual std::uint64_t GetMediaGeneration() const { return 0; }
};
