using System;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using WinUIApp1.Controls;
using WinUIApp1.Dialogs;
using WinUIApp1.Helpers;
using WinUIApp1.Services;
using WinUIApp1.ViewModels;

namespace WinUIApp1;

/// <summary>
/// 메인 윈도우 - 카메라 스트리밍/녹화/재생 뷰어
/// </summary>
public sealed partial class MainWindow : Window
{
    private readonly DispatcherTimer _hideControlBarTimer;
    private readonly DispatcherTimer _clockTimer;
    private bool _isControlBarVisible;
    private bool _isStreamingMode = true;
    private bool _isPlaying = false;

    // Services (간소화된 버전 - 실제 Media Foundation 없이 UI 테스트용)
    private readonly ConfigService _configService = new();

    public MainWindow()
    {
        InitializeComponent();

        // 타이머 설정
        _hideControlBarTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(1)
        };
        _hideControlBarTimer.Tick += (s, e) =>
        {
            HideControlBar();
            _hideControlBarTimer.Stop();
        };

        _clockTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(1)
        };
        _clockTimer.Tick += (s, e) =>
        {
            CurrentTimeText.Text = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
        };
        _clockTimer.Start();

        // 창 설정
        this.Title = "카메라 뷰어";

        // 이벤트 핸들러
        this.Closed += MainWindow_Closed;

        // 초기 UI 상태 설정
        UpdateModeUI();
        UpdateConnectionUI(false);
        UpdateRecordingUI(false);
        NoSignalOverlay.Visibility = Visibility.Visible;
        StatusText.Text = "스트리밍 모드 - 카메라 연결 대기 중";

        // 시계 시작
        CurrentTimeText.Text = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
    }

    /// <summary>
    /// 마우스 이동 처리 (하단 호버 시 컨트롤바 표시)
    /// </summary>
    private void RootGrid_PointerMoved(object sender, PointerRoutedEventArgs e)
    {
        var position = e.GetCurrentPoint(RootGrid).Position;
        var height = RootGrid.ActualHeight;

        // 화면 하단 100픽셀 영역에서 컨트롤바 표시
        if (position.Y > height - 100)
        {
            ShowControlBar();
            _hideControlBarTimer.Stop();
        }
        else
        {
            // 하단 영역을 벗어나면 타이머 시작 (1초 후 숨김)
            if (_isControlBarVisible && !_hideControlBarTimer.IsEnabled)
            {
                _hideControlBarTimer.Start();
            }
        }
    }

    /// <summary>
    /// 마우스가 창을 벗어나면 컨트롤바 숨기기
    /// </summary>
    private void RootGrid_PointerExited(object sender, PointerRoutedEventArgs e)
    {
        _hideControlBarTimer.Stop();
        HideControlBar();
    }

    /// <summary>
    /// 컨트롤바 표시
    /// </summary>
    private void ShowControlBar()
    {
        if (_isControlBarVisible) return;
        _isControlBarVisible = true;
        ControlBar.Opacity = 1;
    }

    /// <summary>
    /// 컨트롤바 숨기기
    /// </summary>
    private void HideControlBar()
    {
        _isControlBarVisible = false;
        ControlBar.Opacity = 0;
    }

    /// <summary>
    /// 연결 상태 UI 업데이트
    /// </summary>
    private void UpdateConnectionUI(bool connected)
    {
        if (connected)
        {
            ConnectionIndicator.Fill = new SolidColorBrush(Microsoft.UI.Colors.LimeGreen);
            ConnectionText.Text = "연결됨";
        }
        else
        {
            ConnectionIndicator.Fill = new SolidColorBrush(Microsoft.UI.Colors.Gray);
            ConnectionText.Text = "연결 안됨";
        }
    }

    /// <summary>
    /// 녹화 상태 UI 업데이트
    /// </summary>
    private void UpdateRecordingUI(bool recording)
    {
        RecordingIndicator.Visibility = recording
            ? Visibility.Visible
            : Visibility.Collapsed;
    }

    /// <summary>
    /// 모드 UI 업데이트
    /// </summary>
    private void UpdateModeUI()
    {
        ControlBar.IsStreamingMode = _isStreamingMode;
        ControlBar.IsPlaying = _isPlaying;

        if (_isStreamingMode)
        {
            StatusText.Text = "스트리밍 모드";
        }
        else
        {
            StatusText.Text = "재생 모드 - 불러오기를 클릭하세요";
        }
    }

    // 컨트롤바 이벤트 핸들러
    private void ControlBar_ModeToggleRequested(object? sender, EventArgs e)
    {
        // 모드 토글
        _isStreamingMode = !_isStreamingMode;
        _isPlaying = false;
        ControlBar.HasLoadedFile = false;

        UpdateModeUI();

        if (_isStreamingMode)
        {
            NoSignalOverlay.Visibility = Visibility.Visible;
            StatusText.Text = "스트리밍 모드 - 카메라 연결 대기 중";
        }
        else
        {
            NoSignalOverlay.Visibility = Visibility.Visible;
            StatusText.Text = "재생 모드 - 불러오기를 클릭하세요";
        }
    }

    private async void ControlBar_LoadRequested(object? sender, EventArgs e)
    {
        if (_isStreamingMode) return;

        try
        {
            // 날짜/시간 선택 다이얼로그 표시
            var dialog = new DateTimePickerDialog
            {
                XamlRoot = this.Content.XamlRoot
            };

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary && dialog.IsValidSelection)
            {
                // 파일 불러오기 시뮬레이션
                StatusText.Text = $"파일 불러오는 중... ({dialog.SelectedDateTime:yyyy-MM-dd HH:mm:ss})";
                NoSignalOverlay.Visibility = Visibility.Visible;

                // TODO: 실제 파일 로드 로직
                await Task.Delay(500); // 시뮬레이션

                StatusText.Text = $"재생 준비 완료 ({dialog.SelectedDateTime:HH:mm:ss})";
                ControlBar.PlaybackTime = dialog.SelectedDateTime;
                ControlBar.HasLoadedFile = true;
                NoSignalOverlay.Visibility = Visibility.Collapsed;
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"불러오기 실패: {ex.Message}";
        }
    }

    private void ControlBar_PlayPauseRequested(object? sender, EventArgs e)
    {
        if (_isStreamingMode) return;

        _isPlaying = !_isPlaying;
        ControlBar.IsPlaying = _isPlaying;

        if (_isPlaying)
        {
            StatusText.Text = "재생 중";
        }
        else
        {
            StatusText.Text = "일시정지";
        }
    }

    private void ControlBar_SeekRequested(object? sender, TimeSpan position)
    {
        if (_isStreamingMode) return;

        ControlBar.CurrentPosition = position;
        StatusText.Text = $"위치 이동: {position:mm\\:ss}";
    }

    private async void ControlBar_TimeJumpRequested(object? sender, EventArgs e)
    {
        if (_isStreamingMode) return;

        try
        {
            // 시간 이동 다이얼로그 표시
            var dialog = new DateTimePickerDialog
            {
                XamlRoot = this.Content.XamlRoot,
                Title = "시간 이동"
            };

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary && dialog.IsValidSelection)
            {
                StatusText.Text = $"시간 이동: {dialog.SelectedDateTime:HH:mm:ss}";
                ControlBar.PlaybackTime = dialog.SelectedDateTime;
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"시간 이동 실패: {ex.Message}";
        }
    }

    /// <summary>
    /// 창 종료 처리
    /// </summary>
    private void MainWindow_Closed(object sender, WindowEventArgs args)
    {
        _clockTimer.Stop();
        _hideControlBarTimer.Stop();
    }
}
