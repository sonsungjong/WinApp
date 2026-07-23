using System.Configuration;
using System.Data;
using System.Windows;
using EOIRUI.Services;
using EOIRUI.Views;
using EOIRUI.ViewModels;

namespace EOIRUI
{
    public partial class App : Application
    {
        private MainViewModel? _mainViewModel;

        protected override async void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            var configService = new ConfigService();
            Models.AppConfig config;

            try
            {
                config = await configService.LoadOrCreateAsync();
            }
            catch (Exception exception)
            {
                MessageBox.Show(
                    $"설정 파일을 읽을 수 없습니다.\n{configService.ConfigPath}\n\n{exception.Message}",
                    "EOIRUI 설정 오류",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                Shutdown(-1);
                return;
            }

            var cameraDataUdpServer = new CameraDataUdpServer(config);
            _mainViewModel = new MainViewModel(cameraDataUdpServer, config);

            MainWindow = new MainView
            {
                DataContext = _mainViewModel
            };
            MainWindow.Show();

            await _mainViewModel.InitializeAsync();
        }

        protected override void OnExit(ExitEventArgs e)
        {
            _mainViewModel?.Dispose();
            base.OnExit(e);
        }
    }
}
