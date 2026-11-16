# CppMcpApp MVP boundaries

- `MainModel`, `MainView`, `MainPresenter` form the application shell: top-level window, common menus, application lifetime and hosted-screen layout.
- `HomeModel`, `HomeView`, `HomePresenter` implement the Home screen inside that shell. Add Home-screen state, controls and behavior to these classes, not to Main.
- `SettingModel`, `SettingView`, `SettingPresenter` own the modal settings popup. Settings controls and behavior belong here, not in Main or Home.
- Presenters use `IMainView` / `IHomeView` / `ISettingView`. Models have no Win32 or view dependency.
- Compose and connect the MVP objects in `CppMcpApp.cpp`. `MainPresenter` may initialize `HomePresenter` or open `SettingPresenter`, but must not implement screen behavior.
- Keep the current work to the requested scaffold. Add MCP, Hermes integration or dashboard features only when requested.
