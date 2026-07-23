using System.Collections.ObjectModel;
using System.Text;
using System.Windows.Input;
using EOIRUI.Models;
using EOIRUI.Services;

namespace EOIRUI.ViewModels;

public sealed class MainViewModel : ViewModelBase, IDisposable
{
    private const int MaxLogEntries = 500;

    private readonly IUdpService _udpService;
    private readonly SynchronizationContext? _uiContext;
    private readonly AsyncRelayCommand _startCommand;
    private readonly AsyncRelayCommand _stopCommand;
    private readonly AsyncRelayCommand _sendCommand;
    private readonly RelayCommand _clearLogCommand;
    private string _localPort = "9000";
    private string _remoteHost = "127.0.0.1";
    private string _remotePort = "9001";
    private string _outgoingMessage = string.Empty;
    private string _statusMessage = "닫힘";
    private bool _isListening;
    private bool _isDisposed;

    public MainViewModel(IUdpService udpService)
    {
        _udpService = udpService ?? throw new ArgumentNullException(nameof(udpService));
        _uiContext = SynchronizationContext.Current;

        _udpService.DatagramReceived += OnDatagramReceived;
        _udpService.ReceiveFaulted += OnReceiveFaulted;

        _startCommand = new AsyncRelayCommand(_ => StartAsync(), _ => CanStart());
        _stopCommand = new AsyncRelayCommand(_ => StopAsync(), _ => IsListening);
        _sendCommand = new AsyncRelayCommand(_ => SendAsync(), _ => CanSend());
        _clearLogCommand = new RelayCommand(_ => ClearLog(), _ => LogEntries.Count > 0);
    }

    public ObservableCollection<UdpLogEntry> LogEntries { get; } = [];

    public ICommand StartCommand => _startCommand;

    public ICommand StopCommand => _stopCommand;

    public ICommand SendCommand => _sendCommand;

    public ICommand ClearLogCommand => _clearLogCommand;

    public string LocalPort
    {
        get => _localPort;
        set
        {
            if (SetProperty(ref _localPort, value))
            {
                RaiseCommandStates();
            }
        }
    }

    public string RemoteHost
    {
        get => _remoteHost;
        set
        {
            if (SetProperty(ref _remoteHost, value))
            {
                RaiseCommandStates();
            }
        }
    }

    public string RemotePort
    {
        get => _remotePort;
        set
        {
            if (SetProperty(ref _remotePort, value))
            {
                RaiseCommandStates();
            }
        }
    }

    public string OutgoingMessage
    {
        get => _outgoingMessage;
        set
        {
            if (SetProperty(ref _outgoingMessage, value))
            {
                RaiseCommandStates();
            }
        }
    }

    public string StatusMessage
    {
        get => _statusMessage;
        private set => SetProperty(ref _statusMessage, value);
    }

    public bool IsListening
    {
        get => _isListening;
        private set
        {
            if (SetProperty(ref _isListening, value))
            {
                OnPropertyChanged(nameof(IsNotListening));
                RaiseCommandStates();
            }
        }
    }

    public bool IsNotListening => !IsListening;

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        _udpService.DatagramReceived -= OnDatagramReceived;
        _udpService.ReceiveFaulted -= OnReceiveFaulted;
        _udpService.Dispose();
    }

    private bool CanStart() => !IsListening && TryParsePort(LocalPort, out _);

    private bool CanSend()
    {
        return IsListening
            && !string.IsNullOrWhiteSpace(RemoteHost)
            && TryParsePort(RemotePort, out _)
            && !string.IsNullOrEmpty(OutgoingMessage);
    }

    private async Task StartAsync()
    {
        if (!TryParsePort(LocalPort, out var localPort))
        {
            StatusMessage = "올바른 수신 포트를 입력하세요.";
            return;
        }

        try
        {
            await _udpService.StartAsync(localPort);
            IsListening = true;
            StatusMessage = $"UDP 0.0.0.0:{localPort} 수신 중";
        }
        catch (Exception exception)
        {
            StatusMessage = $"열기 실패: {exception.Message}";
        }
    }

    private async Task StopAsync()
    {
        try
        {
            await _udpService.StopAsync();
            StatusMessage = "닫힘";
        }
        catch (Exception exception)
        {
            StatusMessage = $"닫기 실패: {exception.Message}";
        }
        finally
        {
            IsListening = false;
        }
    }

    private async Task SendAsync()
    {
        if (!TryParsePort(RemotePort, out var remotePort))
        {
            StatusMessage = "올바른 송신 포트를 입력하세요.";
            return;
        }

        var message = OutgoingMessage;
        var data = Encoding.UTF8.GetBytes(message);

        try
        {
            await _udpService.SendAsync(data, RemoteHost, remotePort);
            AddLog(new UdpLogEntry(
                DateTime.Now,
                "TX",
                $"{RemoteHost}:{remotePort}",
                message));
            StatusMessage = $"{data.Length}바이트 송신 완료";
        }
        catch (Exception exception)
        {
            StatusMessage = $"송신 실패: {exception.Message}";
        }
    }

    private void OnDatagramReceived(object? sender, UdpDatagramReceivedEventArgs e)
    {
        var message = Encoding.UTF8.GetString(e.Data);
        RunOnUiThread(() =>
        {
            AddLog(new UdpLogEntry(
                DateTime.Now,
                "RX",
                e.RemoteEndPoint.ToString(),
                message));
            StatusMessage = $"{e.Data.Length}바이트 수신";
        });
    }

    private void OnReceiveFaulted(object? sender, Exception exception)
    {
        RunOnUiThread(() => _ = HandleReceiveFaultAsync(exception));
    }

    private async Task HandleReceiveFaultAsync(Exception exception)
    {
        try
        {
            await _udpService.StopAsync();
        }
        finally
        {
            IsListening = false;
            StatusMessage = $"수신 오류: {exception.Message}";
        }
    }

    private void AddLog(UdpLogEntry entry)
    {
        LogEntries.Insert(0, entry);

        while (LogEntries.Count > MaxLogEntries)
        {
            LogEntries.RemoveAt(LogEntries.Count - 1);
        }

        _clearLogCommand.RaiseCanExecuteChanged();
    }

    private void ClearLog()
    {
        LogEntries.Clear();
        _clearLogCommand.RaiseCanExecuteChanged();
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
        _startCommand.RaiseCanExecuteChanged();
        _stopCommand.RaiseCanExecuteChanged();
        _sendCommand.RaiseCanExecuteChanged();
    }

    private static bool TryParsePort(string value, out int port)
    {
        return int.TryParse(value, out port) && port is >= 1 and <= 65535;
    }
}
