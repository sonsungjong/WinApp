using System.Windows.Input;
using EOIRUI.Models;
using EOIRUI.Services;

namespace EOIRUI.ViewModels;

public sealed class MainViewModel : ViewModelBase, IDisposable
{
    private readonly ICameraDataUdpServer _cameraDataUdpServer;
    private readonly IRtspVideoService _rtspVideoService;
    private readonly AppConfig _config;
    private readonly SynchronizationContext? _uiContext;
    private readonly AsyncRelayCommand _startServerCommand;
    private readonly AsyncRelayCommand _stopServerCommand;
    private string _statusMessage = "UDP 서버 시작 준비";
    private bool _isServerRunning;
    private bool _isBusy;
    private bool _isDisposed;
    private int _statisticsRefreshScheduled;
    private long _eoDataPackets;
    private long _eoDataBytes;
    private long _irDataPackets;
    private long _irDataBytes;

    public MainViewModel(
        ICameraDataUdpServer cameraDataUdpServer,
        IRtspVideoService rtspVideoService,
        AppConfig config)
    {
        _cameraDataUdpServer = cameraDataUdpServer ?? throw new ArgumentNullException(nameof(cameraDataUdpServer));
        _rtspVideoService = rtspVideoService ?? throw new ArgumentNullException(nameof(rtspVideoService));
        _config = config ?? throw new ArgumentNullException(nameof(config));
        _uiContext = SynchronizationContext.Current;

        EoCamera = new CameraFeedViewModel(
            "EO CAMERA",
            rtspVideoService.EoMediaPlayer,
            config.EoCvIp,
            config.EoCvPort,
            config.EoDataIp,
            config.EoDataPort);
        IrCamera = new CameraFeedViewModel(
            "IR CAMERA",
            rtspVideoService.IrMediaPlayer,
            config.IrCvIp,
            config.IrCvPort,
            config.IrDataIp,
            config.IrDataPort);

        _cameraDataUdpServer.PacketReceived += OnPacketReceived;
        _cameraDataUdpServer.ListenerFaulted += OnListenerFaulted;
        _rtspVideoService.StreamStateChanged += OnRtspStreamStateChanged;

        _startServerCommand = new AsyncRelayCommand(_ => StartServerAsync(), _ => CanStartServer());
        _stopServerCommand = new AsyncRelayCommand(_ => StopServerAsync(), _ => CanStopServer());
    }

    public CameraFeedViewModel EoCamera { get; }

    public CameraFeedViewModel IrCamera { get; }

    public ICommand StartServerCommand => _startServerCommand;

    public ICommand StopServerCommand => _stopServerCommand;

    public string StatusMessage
    {
        get => _statusMessage;
        private set => SetProperty(ref _statusMessage, value);
    }

    public bool IsServerRunning
    {
        get => _isServerRunning;
        private set
        {
            if (SetProperty(ref _isServerRunning, value))
            {
                RaiseCommandStates();
            }
        }
    }

    public Task InitializeAsync() => StartServerAsync();

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        _cameraDataUdpServer.PacketReceived -= OnPacketReceived;
        _cameraDataUdpServer.ListenerFaulted -= OnListenerFaulted;
        _rtspVideoService.StreamStateChanged -= OnRtspStreamStateChanged;
        _rtspVideoService.Dispose();
        _cameraDataUdpServer.Dispose();
    }

    private bool CanStartServer() => !IsServerRunning && !_isBusy;

    private bool CanStopServer() => IsServerRunning && !_isBusy;

    private async Task StartServerAsync()
    {
        if (!CanStartServer())
        {
            return;
        }

        _isBusy = true;
        RaiseCommandStates();
        StatusMessage = "EO/IR RTSP와 데이터 UDP 수신을 시작하는 중...";

        try
        {
            await _cameraDataUdpServer.StartAsync();
            await _rtspVideoService.StartAsync();
            IsServerRunning = true;
            StatusMessage = $"수신 동작 중 · RTSP {_config.EoCvPort}/{_config.IrCvPort} · DATA {_config.EoDataPort}/{_config.IrDataPort}";
        }
        catch (Exception exception)
        {
            await StopServicesAfterStartFailureAsync();
            IsServerRunning = false;
            StatusMessage = $"수신 시작 실패: {exception.Message}";
        }
        finally
        {
            _isBusy = false;
            RaiseCommandStates();
        }
    }

    private async Task StopServerAsync()
    {
        if (!CanStopServer())
        {
            return;
        }

        _isBusy = true;
        RaiseCommandStates();

        try
        {
            await Task.WhenAll(
                _rtspVideoService.StopAsync(),
                _cameraDataUdpServer.StopAsync());
            IsServerRunning = false;
            StatusMessage = "RTSP/UDP 수신 중지됨";
        }
        catch (Exception exception)
        {
            StatusMessage = $"UDP 서버 중지 실패: {exception.Message}";
        }
        finally
        {
            _isBusy = false;
            RaiseCommandStates();
        }
    }

    private void OnPacketReceived(object? sender, CameraDataPacketEventArgs e)
    {
        switch (e.Camera)
        {
            case CameraKind.Eo:
                Interlocked.Increment(ref _eoDataPackets);
                Interlocked.Add(ref _eoDataBytes, e.Data.Length);
                break;

            case CameraKind.Ir:
                Interlocked.Increment(ref _irDataPackets);
                Interlocked.Add(ref _irDataBytes, e.Data.Length);
                break;
        }

        ScheduleStatisticsRefresh();
    }

    private void OnListenerFaulted(object? sender, CameraDataListenerFaultedEventArgs e)
    {
        RunOnUiThread(() =>
        {
            var message = $"UDP {e.LocalPort} 수신 오류: {e.Exception.Message}";
            StatusMessage = message;

            switch (e.Camera)
            {
                case CameraKind.Eo:
                    EoCamera.SetDataFault(message);
                    break;
                case CameraKind.Ir:
                    IrCamera.SetDataFault(message);
                    break;
            }
        });
    }

    private void OnRtspStreamStateChanged(object? sender, RtspStreamStateChangedEventArgs e)
    {
        RunOnUiThread(() =>
        {
            var camera = e.Camera == CameraKind.Eo ? EoCamera : IrCamera;
            camera.SetVideoStatus(e.Message);

            if (e.State == RtspStreamState.Faulted)
            {
                StatusMessage = e.Message;
            }
        });
    }

    private async Task StopServicesAfterStartFailureAsync()
    {
        try
        {
            await Task.WhenAll(
                _rtspVideoService.StopAsync(),
                _cameraDataUdpServer.StopAsync());
        }
        catch
        {
            // 원래 시작 오류를 유지합니다.
        }
    }

    private void ScheduleStatisticsRefresh()
    {
        if (Interlocked.Exchange(ref _statisticsRefreshScheduled, 1) != 0)
        {
            return;
        }

        _ = RefreshStatisticsAsync();
    }

    private async Task RefreshStatisticsAsync()
    {
        await Task.Delay(200).ConfigureAwait(false);

        RunOnUiThread(() =>
        {
            if (_isDisposed)
            {
                Interlocked.Exchange(ref _statisticsRefreshScheduled, 0);
                return;
            }

            Interlocked.Exchange(ref _statisticsRefreshScheduled, 0);

            EoCamera.UpdateDataStatistics(
                Interlocked.Read(ref _eoDataPackets),
                Interlocked.Read(ref _eoDataBytes));
            IrCamera.UpdateDataStatistics(
                Interlocked.Read(ref _irDataPackets),
                Interlocked.Read(ref _irDataBytes));

        });
    }

    private void RunOnUiThread(Action action)
    {
        if (_uiContext is null || SynchronizationContext.Current == _uiContext)
        {
            action();
            return;
        }

        _uiContext.Post(_ => action(), null);
    }

    private void RaiseCommandStates()
    {
        _startServerCommand.RaiseCanExecuteChanged();
        _stopServerCommand.RaiseCanExecuteChanged();
    }
}
