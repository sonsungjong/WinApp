#include "framework.h"
#include "App.h"
#include "Models/ConfigService.h"
#include "Models/ControlBarModel.h"
#include "Models/VideoModel.h"
#include "Presenters/ControlBarPresenter.h"
#include "Presenters/MainPresenter.h"
#include "Presenters/SettingsPresenter.h"
#include "Views/ControlBarView.h"
#include "Views/MainView.h"
#include "Views/SettingsView.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
    {
        return FALSE;
    }

    int exitCode = 0;
    {
        // Models
        ConfigService configService;
        VideoModel videoModel;
        ControlBarModel controlBarModel;

        // Views
        ControlBarView controlBarView;
        SettingsView settingsView;
        MainView mainView(controlBarView);

        if (!mainView.Initialize(hInstance))
        {
            CoUninitialize();
            return FALSE;
        }

        // Presenters
        ControlBarPresenter controlBarPresenter(controlBarView, controlBarModel);
        SettingsPresenter settingsPresenter(settingsView, configService);
        MainPresenter mainPresenter(mainView, videoModel, configService, controlBarPresenter, settingsPresenter);

        if (!mainPresenter.Initialize())
        {
            CoUninitialize();
            return FALSE;
        }

        mainView.Show(nCmdShow);

        MSG msg{};
        bool running = true;
        while (running)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    exitCode = static_cast<int>(msg.wParam);
                    running = false;
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            if (!running)
            {
                break;
            }

            if (mainPresenter.IsActiveLoop())
            {
                mainPresenter.Tick();
                MsgWaitForMultipleObjectsEx(0, nullptr, 33, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
            else
            {
                WaitMessage();
            }
        }
    }

    CoUninitialize();
    return exitCode;
}
