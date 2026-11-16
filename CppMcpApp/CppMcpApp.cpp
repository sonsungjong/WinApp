#include "framework.h"
#include "CppMcpApp.h"
#include "Models/HomeModel.h"
#include "Models/MainModel.h"
#include "Models/SettingModel.h"
#include "Presenters/HomePresenter.h"
#include "Presenters/MainPresenter.h"
#include "Presenters/SettingPresenter.h"
#include "Views/HomeView.h"
#include "Views/MainView.h"
#include "Views/SettingView.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    wchar_t title[128]{};
    if (!LoadStringW(hInstance, IDS_APP_TITLE, title, ARRAYSIZE(title)))
        return 1;

    // Compose the shell, Home screen and settings popup. Views outlive presenters.
    MainModel mainModel(title);
    HomeModel homeModel;
    SettingModel settingModel;
    MainView mainView;
    HomeView homeView;
    if (!mainView.Initialize(hInstance) || !homeView.Initialize(hInstance, mainView.GetHwnd()))
        return 1;
    mainView.SetContentHwnd(homeView.GetHwnd());
    SettingView settingView(hInstance, mainView.GetHwnd());

    HomePresenter homePresenter(homeView, homeModel);
    SettingPresenter settingPresenter(settingView, settingModel);
    MainPresenter mainPresenter(mainView, mainModel, homePresenter, settingPresenter);
    mainPresenter.Initialize();
    mainView.Show(nCmdShow);

    const HACCEL accelerators = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_CPPMCPAPP));
    MSG message{};
    BOOL result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
    {
        if (!TranslateAcceleratorW(mainView.GetHwnd(), accelerators, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return result == -1 ? 1 : static_cast<int>(message.wParam);
}
