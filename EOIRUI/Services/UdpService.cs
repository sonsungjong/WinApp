using System.Net;
using System.Net.Sockets;

namespace EOIRUI.Services;

public sealed class UdpService : IUdpService
{
    private readonly SemaphoreSlim _stateLock = new(1, 1);
    private UdpClient? _client;
    private CancellationTokenSource? _receiveCancellation;
    private Task? _receiveTask;
    private bool _isDisposed;

    public event EventHandler<UdpDatagramReceivedEventArgs>? DatagramReceived;

    public event EventHandler<Exception>? ReceiveFaulted;

    public bool IsRunning => _client is not null;

    public async Task StartAsync(int localPort, CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        ValidatePort(localPort, nameof(localPort));

        await _stateLock.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            if (_client is not null)
            {
                throw new InvalidOperationException("UDP 수신기가 이미 실행 중입니다.");
            }

            var client = new UdpClient(new IPEndPoint(IPAddress.Any, localPort));
            var receiveCancellation = new CancellationTokenSource();

            _client = client;
            _receiveCancellation = receiveCancellation;
            _receiveTask = ReceiveLoopAsync(client, receiveCancellation.Token);
        }
        finally
        {
            _stateLock.Release();
        }
    }

    public async Task StopAsync()
    {
        UdpClient? client;
        CancellationTokenSource? receiveCancellation;
        Task? receiveTask;

        await _stateLock.WaitAsync().ConfigureAwait(false);
        try
        {
            client = _client;
            receiveCancellation = _receiveCancellation;
            receiveTask = _receiveTask;

            _client = null;
            _receiveCancellation = null;
            _receiveTask = null;
        }
        finally
        {
            _stateLock.Release();
        }

        receiveCancellation?.Cancel();
        client?.Dispose();

        if (receiveTask is not null)
        {
            try
            {
                await receiveTask.ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                // 정상적인 수신 루프 종료입니다.
            }
        }

        receiveCancellation?.Dispose();
    }

    public async Task SendAsync(
        byte[] data,
        string remoteHost,
        int remotePort,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(data);
        ArgumentException.ThrowIfNullOrWhiteSpace(remoteHost);
        ValidatePort(remotePort, nameof(remotePort));
        ObjectDisposedException.ThrowIf(_isDisposed, this);

        UdpClient client;

        await _stateLock.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            client = _client
                ?? throw new InvalidOperationException("먼저 UDP 수신 포트를 열어 주세요.");
        }
        finally
        {
            _stateLock.Release();
        }

        var addresses = await Dns.GetHostAddressesAsync(remoteHost, cancellationToken).ConfigureAwait(false);
        var address = addresses.FirstOrDefault(candidate => candidate.AddressFamily == AddressFamily.InterNetwork)
            ?? throw new InvalidOperationException($"IPv4 주소를 찾을 수 없습니다: {remoteHost}");
        var remoteEndPoint = new IPEndPoint(address, remotePort);

        await client.SendAsync(data.AsMemory(), remoteEndPoint, cancellationToken).ConfigureAwait(false);
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        StopAsync().GetAwaiter().GetResult();
        _stateLock.Dispose();
    }

    private async Task ReceiveLoopAsync(UdpClient client, CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var result = await client.ReceiveAsync(cancellationToken).ConfigureAwait(false);
                DatagramReceived?.Invoke(
                    this,
                    new UdpDatagramReceivedEventArgs(result.Buffer, result.RemoteEndPoint));
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            // 정상적인 종료입니다.
        }
        catch (ObjectDisposedException) when (cancellationToken.IsCancellationRequested)
        {
            // StopAsync에서 소켓을 닫은 경우입니다.
        }
        catch (Exception exception)
        {
            ReceiveFaulted?.Invoke(this, exception);
        }
    }

    private static void ValidatePort(int port, string parameterName)
    {
        if (port is < 1 or > IPEndPoint.MaxPort)
        {
            throw new ArgumentOutOfRangeException(parameterName, "포트는 1에서 65535 사이여야 합니다.");
        }
    }
}
