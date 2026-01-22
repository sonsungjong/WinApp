using System;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using WinUIApp1.Models;
using WinUIApp1.Services;

namespace WinUIApp1.ViewModels;

/// <summary>
/// 메인 윈도우 뷰모델
/// </summary>
public partial class MainViewModel : ObservableObject, IDisposable
{
    private readonly ConfigService _configService;
    private readonly CameraConnectionService _cameraConnection;
    private readonly MediaFoundationService _mediaService;
    private readonly RecordingService _recordingService;
    private readonly PlaybackService _playbackService;
    private readonly StorageService _storageService;

    [ObservableProperty]
    private bool _isStreamingMode = true;

    [ObservableProperty]
    private bool _isPlaying;

    [ObservableProperty]
    private bool _isPaused;

    [ObservableProperty]
    private bool _isConnected;

    [ObservableProperty]
    private bool _hasVideoSignal;

    [ObservableProperty]
    private bool _isRecording;

    [ObservableProperty]
    private bool _isControlBarVisible;

    [ObservableProperty]
    private TimeSpan _currentPosition;

    [ObservableProperty]
    private TimeSpan _totalDuration = TimeSpan.FromMinutes(20);

    [ObservableProperty]
    private DateTime _currentPlaybackTime;

    [ObservableProperty]
    private string _statusMessage = "";

    public AppConfig Config => _configService.Config;

    public MainViewModel(
        ConfigService configService,
        CameraConnectionService cameraConnection,
        MediaFoundationService mediaService,
        RecordingService recordingService,
        PlaybackService playbackService,
        StorageService storageService)
    {
        _configService = configService;
        _cameraConnection = cameraConnection;
        _mediaService = mediaService;
        _recordingService = recordingService;
        _playbackService = playbackService;
        _storageService = storageService;

        // 이벤트 핸들러 등록
        _cameraConnection.ConnectionStateChanged += OnConnectionStateChanged;
        _mediaService.StreamError += OnStreamError;
        _mediaService.StreamEnded += OnStreamEnded;
        _mediaService.FrameReceived += OnFrameReceived;
        _playbackService.PlaybackEnded += OnPlaybackEnded;
        _playbackService.PlaybackReady += OnPlaybackReady;
    }

    /// <summary>
    /// 초기화
    /// </summary>
    public async Task InitializeAsync()
    {
        await _configService.LoadAsync();
        _mediaService.Initialize();

        // TCP 연결 시작
        await _cameraConnection.StartAsync(Config.CameraIp, Config.TcpControlPort);

        // 스트리밍 모드로 시작
        if (IsStreamingMode)
        {
            await StartStreamingAsync();
        }
    }

    /// <summary>
    /// 스트리밍 시작
    /// </summary>
    private async Task StartStreamingAsync()
    {
        try
        {
            StatusMessage = "스트리밍 연결 중...";
            await _mediaService.StartStreamingAsync(Config.RtspUrl);
            HasVideoSignal = true;
            StatusMessage = "";

            // 상시 녹화 시작
            _recordingService.StartRecording();
            IsRecording = true;
        }
        catch (Exception ex)
        {
            HasVideoSignal = false;
            StatusMessage = "영상 신호가 없습니다";
        }
    }

    /// <summary>
    /// 스트리밍/재생 모드 전환
    /// </summary>
    [RelayCommand]
    private async Task ToggleModeAsync()
    {
        if (IsStreamingMode)
        {
            // 재생 모드로 전환
            await _mediaService.StopStreamingAsync();
            await _recordingService.StopRecordingAsync();
            IsRecording = false;
            IsStreamingMode = false;
            HasVideoSignal = false;
            StatusMessage = "재생 모드 - 불러오기를 클릭하세요";
        }
        else
        {
            // 스트리밍 모드로 전환
            await _playbackService.CleanupDecryptedFilesAsync();
            IsStreamingMode = true;
            await StartStreamingAsync();
        }
    }

    /// <summary>
    /// 녹화 파일 불러오기
    /// </summary>
    [RelayCommand]
    private async Task LoadRecordingAsync(DateTime selectedTime)
    {
        if (IsStreamingMode) return;

        try
        {
            StatusMessage = "파일 복호화 중...";
            await _playbackService.LoadRecordingsAsync(selectedTime);
            CurrentPlaybackTime = selectedTime;
            StatusMessage = "";
        }
        catch (Exception ex)
        {
            StatusMessage = $"불러오기 실패: {ex.Message}";
        }
    }

    /// <summary>
    /// 재생/일시정지 토글
    /// </summary>
    [RelayCommand]
    private async Task TogglePlayPauseAsync()
    {
        if (IsStreamingMode) return;

        if (IsPlaying)
        {
            _playbackService.Pause();
            IsPlaying = false;
            IsPaused = true;
        }
        else
        {
            await _playbackService.PlayAsync();
            IsPlaying = true;
            IsPaused = false;
        }
    }

    /// <summary>
    /// 시간 이동
    /// </summary>
    [RelayCommand]
    private async Task SeekToTimeAsync(DateTime targetTime)
    {
        if (IsStreamingMode) return;

        var wasPlaying = IsPlaying;

        await _playbackService.StopAsync();
        await _playbackService.CleanupDecryptedFilesAsync();
        await LoadRecordingAsync(targetTime);

        if (wasPlaying)
        {
            await _playbackService.PlayAsync();
            IsPlaying = true;
        }
    }

    /// <summary>
    /// 슬라이더 위치 변경
    /// </summary>
    [RelayCommand]
    private async Task SeekToPositionAsync(TimeSpan position)
    {
        if (IsStreamingMode) return;

        await _playbackService.SeekAsync(position);
        CurrentPosition = position;
    }

    /// <summary>
    /// 컨트롤바 표시/숨김
    /// </summary>
    public void ShowControlBar()
    {
        IsControlBarVisible = true;
    }

    public void HideControlBar()
    {
        IsControlBarVisible = false;
    }

    // 이벤트 핸들러
    private void OnConnectionStateChanged(object? sender, bool connected)
    {
        IsConnected = connected;
        if (!connected && IsStreamingMode)
        {
            HasVideoSignal = false;
            StatusMessage = "영상 신호가 없습니다";
        }
    }

    private void OnStreamError(object? sender, Exception ex)
    {
        HasVideoSignal = false;
        StatusMessage = "영상 신호가 없습니다";
    }

    private void OnStreamEnded(object? sender, EventArgs e)
    {
        if (IsStreamingMode)
        {
            HasVideoSignal = false;
            StatusMessage = "영상 신호가 없습니다";
        }
        else
        {
            IsPlaying = false;
        }
    }

    private void OnFrameReceived(object? sender, VideoFrameEventArgs e)
    {
        HasVideoSignal = true;
        if (StatusMessage == "영상 신호가 없습니다")
        {
            StatusMessage = "";
        }
    }

    private void OnPlaybackEnded(object? sender, EventArgs e)
    {
        IsPlaying = false;
    }

    private void OnPlaybackReady(object? sender, EventArgs e)
    {
        StatusMessage = "재생 준비 완료";
    }

    /// <summary>
    /// 종료 처리
    /// </summary>
    public async Task ShutdownAsync()
    {
        // 녹화 중지
        await _recordingService.StopRecordingAsync();

        // 재생 중지
        await _playbackService.StopAsync();
        await _playbackService.CleanupDecryptedFilesAsync();

        // 스트리밍 중지
        await _mediaService.StopStreamingAsync();

        // TCP 연결 종료
        _cameraConnection.Stop();
    }

    public void Dispose()
    {
        _cameraConnection.ConnectionStateChanged -= OnConnectionStateChanged;
        _mediaService.StreamError -= OnStreamError;
        _mediaService.StreamEnded -= OnStreamEnded;
        _mediaService.FrameReceived -= OnFrameReceived;
        _playbackService.PlaybackEnded -= OnPlaybackEnded;
        _playbackService.PlaybackReady -= OnPlaybackReady;

        _recordingService.Dispose();
        _playbackService.Dispose();
        _mediaService.Dispose();
        _cameraConnection.Dispose();
    }
}
