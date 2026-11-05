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

    VideoModel model;
    MainView view;

    if (!view.Initialize(hInstance))
    {
        return FALSE;
    }

    MainPresenter presenter(view, model);
    presenter.Initialize();

    view.Show(nCmdShow);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
