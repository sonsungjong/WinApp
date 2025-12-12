using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using Microsoft.Web.WebView2.Core;
using WPFWeb.ViewModel;

namespace WPFWeb.View
{
    /// <summary>
    /// MainView.xaml에 대한 상호 작용 논리
    /// </summary>
    public partial class MainView : Window
    {
        private MainViewModel? m_viewModel => DataContext as MainViewModel;

        public MainView()
        {
            InitializeComponent();
            InitializeWebView();
            SubscribeToViewModelEvents();
        }

        private void SubscribeToViewModelEvents()
        {
            if (m_viewModel != null)
            {
                m_viewModel.GoBackRequested += () =>
                {
                    if (webView.CanGoBack)
                        webView.GoBack();
                };

                m_viewModel.GoForwardRequested += () =>
                {
                    if (webView.CanGoForward)
                        webView.GoForward();
                };

                m_viewModel.ReloadRequested += () =>
                {
                    webView.Reload();
                };
            }
        }

        private async void InitializeWebView()
        {
            await webView.EnsureCoreWebView2Async();
            
            webView.NavigationStarting += WebView_NavigationStarting;
            webView.NavigationCompleted += WebView_NavigationCompleted;
            webView.CoreWebView2InitializationCompleted += WebView_CoreWebView2InitializationCompleted;
        }

        private void WebView_CoreWebView2InitializationCompleted(object? sender, CoreWebView2InitializationCompletedEventArgs e)
        {
            if (e.IsSuccess)
            {
                webView.CoreWebView2.HistoryChanged += CoreWebView2_HistoryChanged;
            }
        }

        private void CoreWebView2_HistoryChanged(object? sender, object e)
        {
            m_viewModel?.UpdateNavigationState(webView.CanGoBack, webView.CanGoForward);
        }

        private void WebView_NavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
        {
            if (m_viewModel != null)
                m_viewModel.IsLoading = true;
        }

        private void WebView_NavigationCompleted(object? sender, CoreWebView2NavigationCompletedEventArgs e)
        {
            if (m_viewModel != null)
            {
                m_viewModel.IsLoading = false;
                m_viewModel.UpdateUrl(webView.Source?.ToString() ?? string.Empty);
                m_viewModel.UpdateNavigationState(webView.CanGoBack, webView.CanGoForward);
            }
        }

        private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                DragMove();
            }
        }

        private void Minimize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void Maximize_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
        }

        private void Close_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
