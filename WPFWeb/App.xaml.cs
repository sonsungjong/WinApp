using System.Configuration;
using System.Data;
using System.Windows;
using WPFWeb.View;

namespace WPFWeb
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private void Application_Startup(object sender, StartupEventArgs e)
        {
            var mainView = new MainView();
            mainView.Show();

        }
    }

}
