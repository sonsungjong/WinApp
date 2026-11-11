#include "Services/Api/UiDispatcher.h"
#include "Services/Api/ApiTypes.h"
#include <stdexcept>

namespace
{
constexpr UINT DispatchMessageId = WM_APP + 73;
}
UiDispatcher::UiDispatcher()
{
    WNDCLASSW cls{};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"NextVideoPlayer_ApiDispatcher";
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw std::runtime_error("Cannot register UI dispatcher");
    m_window = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, this);
    if (!m_window)
        throw std::runtime_error("Cannot create UI dispatcher");
}
UiDispatcher::~UiDispatcher()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopped = true;
        m_actions.clear();
    }
    if (m_window)
        DestroyWindow(m_window);
}
bool UiDispatcher::Post(std::function<void()> action)
{
    std::lock_guard lock(m_mutex);
    if (m_stopped || m_actions.size() >= Api::MaxPendingCommands)
        return false;
    m_actions.push_back(std::move(action));
    if (!PostMessageW(m_window, DispatchMessageId, 0, 0))
    {
        m_actions.pop_back();
        return false;
    }
    return true;
}
void UiDispatcher::Drain()
{
    // Yield to painting/input/timers between small command batches.
    for (int i = 0; i < 8; ++i)
    {
        std::function<void()> action;
        {
            std::lock_guard lock(m_mutex);
            if (m_actions.empty())
                return;
            action = std::move(m_actions.front());
            m_actions.pop_front();
        }
        try
        {
            action();
        }
        catch (const std::exception &error)
        {
            OutputDebugStringA(error.what());
        }
    }
    std::lock_guard lock(m_mutex);
    if (!m_actions.empty())
        PostMessageW(m_window, DispatchMessageId, 0, 0);
}
LRESULT CALLBACK UiDispatcher::WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_NCCREATE)
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams));
    auto *self = reinterpret_cast<UiDispatcher *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self && message == DispatchMessageId)
    {
        self->Drain();
        return 0;
    }
    if (message == WM_NCDESTROY)
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return DefWindowProcW(window, message, w, l);
}
