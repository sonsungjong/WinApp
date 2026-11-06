#pragma once
#include "framework.h"

class IPlaylistView
{
public:
    virtual ~IPlaylistView() = default;
    virtual bool Initialize(HINSTANCE instance, HWND owner) = 0;
    virtual bool IsVisible() const = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void Render(const std::vector<std::wstring>& items, int currentIndex) = 0;
    virtual std::vector<std::wstring> PromptOpenFiles() = 0;
    virtual void SetOnPlay(std::function<void(const std::vector<int>&)> handler) = 0;
    virtual void SetOnAdd(std::function<void()> handler) = 0;
    virtual void SetOnRemove(std::function<void(const std::vector<int>&)> handler) = 0;
    virtual void SetOnClear(std::function<void()> handler) = 0;
    virtual void SetOnVisibilityChanged(std::function<void()> handler) = 0;
};
