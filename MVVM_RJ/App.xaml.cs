using System.Configuration;
using System.Data;
using System.Runtime.InteropServices;
using System.Windows;

namespace MVVM_RJ
{
    public partial class App : Application
    {
        private void Application_Startup(object sender, StartupEventArgs e)
        {
            var loginView = new View.LoginView();
            loginView.Show();
            loginView.IsVisibleChanged += (s, ev) =>
            {
                if (loginView.IsVisible == false && loginView.IsLoaded)
                {
                    var mainView = new View.MainView();
                    mainView.Show();
                    loginView.Close();
                }
            };
        }
    }
}
