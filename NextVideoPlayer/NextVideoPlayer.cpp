#include "framework.h"
#include "NextVideoPlayer.h"
#include "Models/VideoModel.h"
#include "Presenters/MainPresenter.h"
#include "Views/MainView.h"

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
        VideoModel model;
        MainView view;

        if (!view.Initialize(hInstance))
        {
            CoUninitialize();
            return FALSE;
        }

        MainPresenter presenter(view, model);
        presenter.Initialize();

        view.Show(nCmdShow);

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

            if (presenter.IsPlaying())
            {
                presenter.Tick();
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
