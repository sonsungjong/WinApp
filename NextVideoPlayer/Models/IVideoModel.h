#pragma once

#include "framework.h"

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

    virtual bool OpenFile(const std::wstring& filePath) = 0;
    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void Stop() = 0;

    virtual void SetRepeat(bool enabled) = 0;
    virtual bool IsRepeatEnabled() const = 0;

    virtual PlaybackState GetState() const = 0;
    virtual const std::wstring& GetFilePath() const = 0;
    virtual bool HasMedia() const = 0;
};

