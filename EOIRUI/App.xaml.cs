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

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            var udpService = new UdpService();
            _mainViewModel = new MainViewModel(udpService);

            MainWindow = new MainView
            {
                DataContext = _mainViewModel
            };
            MainWindow.Show();
        }

        protected override void OnExit(ExitEventArgs e)
        {
            _mainViewModel?.Dispose();
            base.OnExit(e);
        }
    }
}
