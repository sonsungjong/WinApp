namespace EOIRUI.Services;

public interface ICameraDataUdpServer : IDisposable
{
    event EventHandler<CameraDataPacketEventArgs>? PacketReceived;

    event EventHandler<CameraDataListenerFaultedEventArgs>? ListenerFaulted;

    bool IsRunning { get; }

    Task StartAsync(CancellationToken cancellationToken = default);

    Task StopAsync();
}
