using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace WinUIApp1.Controls;

/// <summary>
/// 컨트롤바 사용자 컨트롤
/// </summary>
public sealed partial class ControlBar : UserControl
{
    // 의존성 속성
    public static readonly DependencyProperty IsStreamingModeProperty =
        DependencyProperty.Register(nameof(IsStreamingMode), typeof(bool), typeof(ControlBar),
            new PropertyMetadata(true, OnModeChanged));

    public static readonly DependencyProperty IsPlayingProperty =
        DependencyProperty.Register(nameof(IsPlaying), typeof(bool), typeof(ControlBar),
            new PropertyMetadata(false, OnPlayStateChanged));

    public static readonly DependencyProperty CurrentPositionProperty =
        DependencyProperty.Register(nameof(CurrentPosition), typeof(TimeSpan), typeof(ControlBar),
            new PropertyMetadata(TimeSpan.Zero, OnPositionChanged));

    public static readonly DependencyProperty TotalDurationProperty =
        DependencyProperty.Register(nameof(TotalDuration), typeof(TimeSpan), typeof(ControlBar),
            new PropertyMetadata(TimeSpan.FromMinutes(20)));

    public static readonly DependencyProperty PlaybackTimeProperty =
        DependencyProperty.Register(nameof(PlaybackTime), typeof(DateTime), typeof(ControlBar),
            new PropertyMetadata(DateTime.Now));

    public static readonly DependencyProperty HasLoadedFileProperty =
        DependencyProperty.Register(nameof(HasLoadedFile), typeof(bool), typeof(ControlBar),
            new PropertyMetadata(false, OnModeChanged));

    // 속성
    public bool IsStreamingMode
    {
        get => (bool)GetValue(IsStreamingModeProperty);
        set => SetValue(IsStreamingModeProperty, value);
    }

    public bool IsPlaying
    {
        get => (bool)GetValue(IsPlayingProperty);
        set => SetValue(IsPlayingProperty, value);
    }

    public TimeSpan CurrentPosition
    {
        get => (TimeSpan)GetValue(CurrentPositionProperty);
        set => SetValue(CurrentPositionProperty, value);
    }

    public TimeSpan TotalDuration
    {
        get => (TimeSpan)GetValue(TotalDurationProperty);
        set => SetValue(TotalDurationProperty, value);
    }

    public DateTime PlaybackTime
    {
        get => (DateTime)GetValue(PlaybackTimeProperty);
        set => SetValue(PlaybackTimeProperty, value);
    }

    public bool HasLoadedFile
    {
        get => (bool)GetValue(HasLoadedFileProperty);
        set => SetValue(HasLoadedFileProperty, value);
    }

    // 계산 속성
    public bool IsPlaybackMode => !IsStreamingMode;
    public string ModeIcon => IsStreamingMode ? "\uE714" : "\uE768"; // 카메라/재생 아이콘
    public string ModeText => IsStreamingMode ? "스트리밍" : "재생";
    public double CurrentSeconds => CurrentPosition.TotalSeconds;
    public double TotalSeconds => TotalDuration.TotalSeconds;
    public string CurrentTimeString => CurrentPosition.ToString(@"mm\:ss");
    public string TotalTimeString => TotalDuration.ToString(@"mm\:ss");
    public string PlaybackTimeString => PlaybackTime.ToString("HH:mm:ss");

    // 이벤트
    public event EventHandler? ModeToggleRequested;
    public event EventHandler? LoadRequested;
    public event EventHandler? PlayPauseRequested;
    public event EventHandler<TimeSpan>? SeekRequested;
    public event EventHandler? TimeJumpRequested;

    private FontIcon? _playPauseIcon;

    public ControlBar()
    {
        InitializeComponent();

        // PlayPauseIcon 찾기
        this.Loaded += (s, e) =>
        {
            _playPauseIcon = FindName("PlayPauseIcon") as FontIcon;
            UpdatePlayPauseIcon();
        };
    }

    private static void OnModeChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is ControlBar control)
        {
            control.UpdateUI();
        }
    }

    private static void OnPlayStateChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is ControlBar control)
        {
            control.UpdatePlayPauseIcon();
        }
    }

    private static void OnPositionChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is ControlBar control)
        {
            control.UpdateUI();
        }
    }

    private void UpdateUI()
    {
        // Bindings 업데이트
        try
        {
            Bindings.Update();
        }
        catch
        {
            // 초기화 전에 호출될 수 있음
        }
    }

    private void UpdatePlayPauseIcon()
    {
        if (_playPauseIcon != null)
        {
            _playPauseIcon.Glyph = IsPlaying ? "\uE769" : "\uE768"; // 일시정지/재생 아이콘
        }
    }

    private void ModeToggle_Click(object sender, RoutedEventArgs e)
    {
        ModeToggleRequested?.Invoke(this, EventArgs.Empty);
    }

    private void LoadButton_Click(object sender, RoutedEventArgs e)
    {
        LoadRequested?.Invoke(this, EventArgs.Empty);
    }

    private void PlayPauseButton_Click(object sender, RoutedEventArgs e)
    {
        PlayPauseRequested?.Invoke(this, EventArgs.Empty);
    }

    private void TimelineSlider_ValueChanged(object sender, Microsoft.UI.Xaml.Controls.Primitives.RangeBaseValueChangedEventArgs e)
    {
        if (Math.Abs(e.NewValue - e.OldValue) > 1) // 사용자가 드래그한 경우
        {
            SeekRequested?.Invoke(this, TimeSpan.FromSeconds(e.NewValue));
        }
    }

    private void SeekButton_Click(object sender, RoutedEventArgs e)
    {
        TimeJumpRequested?.Invoke(this, EventArgs.Empty);
    }
}
