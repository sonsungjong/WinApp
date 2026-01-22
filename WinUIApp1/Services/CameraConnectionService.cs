using System;
using System.Net.Sockets;
using System.Net;
using System.Threading;
using System.Threading.Tasks;

namespace WinUIApp1.Services;

/// <summary>
/// 카메라 TCP/IP 제어 연결 서비스
/// - 연결 상태 관리
/// - 자동 재연결
/// - 다른 로직에 영향 없이 백그라운드 동작
/// </summary>
public class CameraConnectionService : IDisposable
{
    private TcpClient? _tcpClient;
    private NetworkStream? _networkStream;
    private CancellationTokenSource? _reconnectCts;
    private readonly object _lock = new();
    private bool _isDisposed;

    public string CameraIp { get; set; } = "";
    public int ControlPort { get; set; } = 8000;

    /// <summary>
    /// 연결 상태
    /// </summary>
    public bool IsConnected => _tcpClient?.Connected ?? false;

    /// <summary>
    /// 연결 상태 변경 이벤트
    /// </summary>
    public event EventHandler<bool>? ConnectionStateChanged;

    /// <summary>
    /// 데이터 수신 이벤트
    /// </summary>
    public event EventHandler<byte[]>? DataReceived;

    /// <summary>
    /// TCP 연결 시작 (백그라운드 자동 재연결 포함)
    /// </summary>
    public async Task StartAsync(string cameraIp, int controlPort)
    {
        CameraIp = cameraIp;
        ControlPort = controlPort;

        _reconnectCts = new CancellationTokenSource();
        await ConnectAsync();

        // 백그라운드 재연결 태스크 시작
        _ = Task.Run(() => ReconnectLoopAsync(_reconnectCts.Token));
    }

    /// <summary>
    /// TCP 연결 시도
    /// </summary>
    private async Task<bool> ConnectAsync()
    {
        if (_isDisposed) return false;

        try
        {
            lock (_lock)
            {
                _tcpClient?.Dispose();
                _tcpClient = new TcpClient();
            }

            await _tcpClient.ConnectAsync(CameraIp, ControlPort);
            _networkStream = _tcpClient.GetStream();

            ConnectionStateChanged?.Invoke(this, true);

            // 수신 루프 시작
            _ = Task.Run(ReceiveLoopAsync);

            return true;
        }
        catch (Exception)
        {
            ConnectionStateChanged?.Invoke(this, false);
            return false;
        }
    }

    /// <summary>
    /// 자동 재연결 루프 (다른 로직에 영향 없이 백그라운드 동작)
    /// </summary>
    private async Task ReconnectLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested && !_isDisposed)
        {
            try
            {
                await Task.Delay(3000, cancellationToken);

                if (!IsConnected)
                {
                    await ConnectAsync();
                }
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch
            {
                // 재연결 실패해도 계속 시도
            }
        }
    }

    /// <summary>
    /// 데이터 수신 루프
    /// </summary>
    private async Task ReceiveLoopAsync()
    {
        var buffer = new byte[4096];

        try
        {
            while (IsConnected && _networkStream != null)
            {
                var bytesRead = await _networkStream.ReadAsync(buffer);
                if (bytesRead == 0) break;

                var data = new byte[bytesRead];
                Array.Copy(buffer, data, bytesRead);
                DataReceived?.Invoke(this, data);
            }
        }
        catch
        {
            // 연결 끊김
        }
        finally
        {
            ConnectionStateChanged?.Invoke(this, false);
        }
    }

    /// <summary>
    /// 데이터 송신
    /// </summary>
    public async Task<bool> SendAsync(byte[] data)
    {
        if (!IsConnected || _networkStream == null) return false;

        try
        {
            await _networkStream.WriteAsync(data);
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// 연결 종료
    /// </summary>
    public void Stop()
    {
        _reconnectCts?.Cancel();

        lock (_lock)
        {
            _networkStream?.Dispose();
            _networkStream = null;
            _tcpClient?.Dispose();
            _tcpClient = null;
        }

        ConnectionStateChanged?.Invoke(this, false);
    }

    public void Dispose()
    {
        if (_isDisposed) return;
        _isDisposed = true;

        Stop();
        _reconnectCts?.Dispose();
    }
}
