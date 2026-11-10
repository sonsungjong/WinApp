#include "framework.h"
#include "App.h"
#include <exception>
#include "Models/ConfigService.h"
#include "Models/ControlBarModel.h"
#include "Models/PlaylistModel.h"
#include "Models/VideoModel.h"
#include "Presenters/ControlBarPresenter.h"
#include "Presenters/MainPresenter.h"
#include "Presenters/PlaylistPresenter.h"
#include "Presenters/SettingsPresenter.h"
#include "Presenters/VideoPlayerPresenter.h"
#include "Views/ControlBarView.h"
#include "Views/FontHelper.h"
#include "Views/MainView.h"
#include "Views/PlaylistView.h"
#include "Views/SettingsView.h"
#include "Views/VideoPlayerView.h"

namespace
{
    class ComApartment
    {
    public:
        ComApartment() : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
        ~ComApartment() { if (SUCCEEDED(m_result)) CoUninitialize(); }
        ComApartment(const ComApartment&) = delete;
        ComApartment& operator=(const ComApartment&) = delete;
        bool IsInitialized() const { return SUCCEEDED(m_result); }
    private:
        HRESULT m_result;
    };

    class AppTimer
    {
    public:
        explicit AppTimer(HWND hwnd) : m_hwnd(hwnd), m_id(SetTimer(hwnd, 1, 33, nullptr)) {}
        ~AppTimer() { if (m_id && IsWindow(m_hwnd)) KillTimer(m_hwnd, m_id); }
        AppTimer(const AppTimer&) = delete;
        AppTimer& operator=(const AppTimer&) = delete;
        UINT_PTR GetId() const { return m_id; }
    private:
        HWND m_hwnd;
        UINT_PTR m_id;
    };
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nCmdShow)
try
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    ComApartment com;
    if (!com.IsInitialized()) return 1;
    FontHelper::Resources fonts;

    ConfigService configService;
    AppConfig config = configService.Load();
    VideoModel videoModel;
    ControlBarModel controlBarModel;
    PlaylistModel playlistModel;

    ControlBarView controlBarView;
    SettingsView settingsView(fonts.GetDialogFont());
    PlaylistView playlistView(fonts.GetDialogFont());
    MainView mainView;
    VideoPlayerView videoPlayerView(controlBarView);

    if (!mainView.Initialize(hInstance)) return 1;
    if (!videoPlayerView.Initialize(hInstance, mainView.GetHwnd())) return 1;
    mainView.SetPlayerHwnd(videoPlayerView.GetHwnd());

    ControlBarPresenter controlBarPresenter(controlBarView, controlBarModel, videoModel);
    SettingsPresenter settingsPresenter(settingsView, configService);
    PlaylistPresenter playlistPresenter(playlistView, playlistModel, videoModel);
    VideoPlayerPresenter videoPlayerPresenter(
        videoPlayerView, mainView, videoModel, config, configService,
        controlBarPresenter, settingsPresenter, playlistPresenter);
    MainPresenter mainPresenter(mainView, videoPlayerPresenter);
    if (!mainPresenter.Initialize()) return 1;

    AppTimer timer(mainView.GetHwnd());
    if (!timer.GetId()) return 1;
    mainView.SetOnTimer([&](UINT_PTR id) { if (id == timer.GetId()) mainPresenter.Tick(); });
    mainView.Show(nCmdShow);

    MSG message{};
    BOOL result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    mainView.SetOnTimer({});
    return result == -1 ? 1 : static_cast<int>(message.wParam);
}
catch (const std::exception& error)
{
    OutputDebugStringA(error.what());
    MessageBoxW(nullptr,
        L"프로그램 초기화에 실패했습니다.\n사용자 설정 폴더의 접근 권한과 실행 파일을 확인해 주세요.",
        L"NextVideoPlayer", MB_OK | MB_ICONERROR);
    return 1;
}
