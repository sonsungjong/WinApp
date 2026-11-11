#pragma once
#include "framework.h"
#include <deque>
#include <mutex>

class UiDispatcher
{
  public:
    UiDispatcher();
    ~UiDispatcher();
    UiDispatcher(const UiDispatcher &) = delete;
    UiDispatcher &operator=(const UiDispatcher &) = delete;
    bool Post(std::function<void()> action);

  private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    void Drain();
    HWND m_window = nullptr;
    std::mutex m_mutex;
    std::deque<std::function<void()>> m_actions;
    bool m_stopped = false;
};
