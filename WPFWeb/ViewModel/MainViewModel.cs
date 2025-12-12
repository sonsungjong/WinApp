using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Input;

namespace WPFWeb.ViewModel
{
    public class MainViewModel : ViewModelBase
    {
        public event Action? GoBackRequested;
        public event Action? GoForwardRequested;
        public event Action? ReloadRequested;

        public MainViewModel()
        {
            NavigateCommand = new ViewModelCommand(ExecuteNavigate);
            GoBackCommand = new ViewModelCommand(ExecuteGoBack, CanExecuteGoBack);
            GoForwardCommand = new ViewModelCommand(ExecuteGoForward, CanExecuteGoForward);
            ReloadCommand = new ViewModelCommand(ExecuteReload);
            HomeCommand = new ViewModelCommand(ExecuteHome);
        }

        public ICommand NavigateCommand { get; }
        public ICommand GoBackCommand { get; }
        public ICommand GoForwardCommand { get; }
        public ICommand ReloadCommand { get; }
        public ICommand HomeCommand { get; }

        private string m_url = "https://www.google.com";
        public string Url
        {
            get => m_url;
            set
            {
                m_url = value;
                OnPropertyChanged(nameof(Url));
            }
        }

        private string m_addressBarText = "https://www.google.com";
        public string AddressBarText
        {
            get => m_addressBarText;
            set
            {
                m_addressBarText = value;
                OnPropertyChanged(nameof(AddressBarText));
            }
        }

        private bool m_canGoBack;
        public bool CanGoBack
        {
            get => m_canGoBack;
            set
            {
                m_canGoBack = value;
                OnPropertyChanged(nameof(CanGoBack));
            }
        }

        private bool m_canGoForward;
        public bool CanGoForward
        {
            get => m_canGoForward;
            set
            {
                m_canGoForward = value;
                OnPropertyChanged(nameof(CanGoForward));
            }
        }

        private bool m_isLoading;
        public bool IsLoading
        {
            get => m_isLoading;
            set
            {
                m_isLoading = value;
                OnPropertyChanged(nameof(IsLoading));
            }
        }

        private string m_titleText = "WPF Web Browser";
        public string TitleText
        {
            get => m_titleText;
            set
            {
                m_titleText = value;
                OnPropertyChanged(nameof(TitleText));
            }
        }


        private void ExecuteNavigate(object? parameter)
        {
            if (!string.IsNullOrWhiteSpace(AddressBarText))
            {
                var urlToNavigate = AddressBarText;

                if (!urlToNavigate.StartsWith("http://") && !urlToNavigate.StartsWith("https://"))
                {
                    urlToNavigate = "https://" + urlToNavigate;
                }

                Url = urlToNavigate;
            }
        }

        private void ExecuteGoBack(object? parameter)
        {
            GoBackRequested?.Invoke();
        }

        private bool CanExecuteGoBack(object? parameter)
        {
            return CanGoBack;
        }

        private void ExecuteGoForward(object? parameter)
        {
            GoForwardRequested?.Invoke();
        }

        private bool CanExecuteGoForward(object? parameter)
        {
            return CanGoForward;
        }

        private void ExecuteReload(object? parameter)
        {
            ReloadRequested?.Invoke();
        }

        private void ExecuteHome(object? parameter)
        {
            Url = "https://www.google.com";
            AddressBarText = Url;
        }

        public void UpdateNavigationState(bool canGoBack, bool canGoForward)
        {
            CanGoBack = canGoBack;
            CanGoForward = canGoForward;
            CommandManager.InvalidateRequerySuggested();
        }

        public void UpdateUrl(string url)
        {
            AddressBarText = url;
        }
    }
}
