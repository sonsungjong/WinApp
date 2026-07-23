namespace EOIRUI.Services;

public interface IUdpService : IDisposable
{
    event EventHandler<UdpDatagramReceivedEventArgs>? DatagramReceived;

    event EventHandler<Exception>? ReceiveFaulted;

    bool IsRunning { get; }

    Task StartAsync(int localPort, CancellationToken cancellationToken = default);

    Task StopAsync();

    Task SendAsync(
        byte[] data,
        string remoteHost,
        int remotePort,
        CancellationToken cancellationToken = default);
}
