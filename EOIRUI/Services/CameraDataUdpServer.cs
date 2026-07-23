using EOIRUI.Models;

namespace EOIRUI.Services;

public sealed class CameraDataUdpServer : ICameraDataUdpServer
{
    private readonly SemaphoreSlim _lifecycleLock = new(1, 1);
    private readonly IReadOnlyList<ListenerRegistration> _listeners;
    private bool _isDisposed;

    public CameraDataUdpServer(AppConfig config)
    {
        ArgumentNullException.ThrowIfNull(config);

        _listeners =
        [
            CreateListener(CameraKind.Eo, config.EoDataPort),
            CreateListener(CameraKind.Ir, config.IrDataPort)
        ];
    }

    public event EventHandler<CameraDataPacketEventArgs>? PacketReceived;

    public event EventHandler<CameraDataListenerFaultedEventArgs>? ListenerFaulted;

    public bool IsRunning { get; private set; }

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);

        await _lifecycleLock.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            if (IsRunning)
            {
                return;
            }

            try
            {
                foreach (var listener in _listeners)
                {
                    await listener.Service
                        .StartAsync(listener.Port, cancellationToken)
                        .ConfigureAwait(false);
                }

                IsRunning = true;
            }
            catch
            {
                await StopListenersAsync().ConfigureAwait(false);
                throw;
            }
        }
        finally
        {
            _lifecycleLock.Release();
        }
    }

    public async Task StopAsync()
    {
        await _lifecycleLock.WaitAsync().ConfigureAwait(false);
        try
        {
            await StopListenersAsync().ConfigureAwait(false);
            IsRunning = false;
        }
        finally
        {
            _lifecycleLock.Release();
        }
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        StopAsync().GetAwaiter().GetResult();

        foreach (var listener in _listeners)
        {
            listener.Service.Dispose();
        }

        _lifecycleLock.Dispose();
    }

    private ListenerRegistration CreateListener(CameraKind camera, int port)
    {
        var service = new UdpService();

        service.DatagramReceived += (_, eventArgs) =>
            PacketReceived?.Invoke(
                this,
                new CameraDataPacketEventArgs(
                    camera,
                    port,
                    eventArgs.Data,
                    eventArgs.RemoteEndPoint));

        service.ReceiveFaulted += (_, exception) =>
            ListenerFaulted?.Invoke(
                this,
                new CameraDataListenerFaultedEventArgs(camera, port, exception));

        return new ListenerRegistration(port, service);
    }

    private Task StopListenersAsync()
    {
        return Task.WhenAll(_listeners.Select(listener => listener.Service.StopAsync()));
    }

    private sealed record ListenerRegistration(int Port, IUdpService Service);
}
