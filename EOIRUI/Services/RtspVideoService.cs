using EOIRUI.Models;
using LibVLCSharp.Shared;

namespace EOIRUI.Services;

public sealed class RtspVideoService : IRtspVideoService
{
    private readonly LibVLC _libVlc;
    private readonly RtspStreamSession _eoSession;
    private readonly RtspStreamSession _irSession;
    private bool _isDisposed;

    public RtspVideoService(AppConfig config)
    {
        ArgumentNullException.ThrowIfNull(config);

        Core.Initialize();
        _libVlc = new LibVLC("--no-audio");
        _eoSession = CreateSession(
            CameraKind.Eo,
            BuildRtspUri(config.EoCvIp, config.EoCvPort));
        _irSession = CreateSession(
            CameraKind.Ir,
            BuildRtspUri(config.IrCvIp, config.IrCvPort));
    }

    public event EventHandler<RtspStreamStateChangedEventArgs>? StreamStateChanged;

    public MediaPlayer EoMediaPlayer => _eoSession.MediaPlayer;

    public MediaPlayer IrMediaPlayer => _irSession.MediaPlayer;

    public Task StartAsync(CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        return Task.WhenAll(
            _eoSession.StartAsync(cancellationToken),
            _irSession.StartAsync(cancellationToken));
    }

    public Task StopAsync()
    {
        return Task.WhenAll(_eoSession.StopAsync(), _irSession.StopAsync());
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        _eoSession.Dispose();
        _irSession.Dispose();
        _libVlc.Dispose();
    }

    private RtspStreamSession CreateSession(CameraKind camera, Uri streamUri)
    {
        var session = new RtspStreamSession(_libVlc, camera, streamUri);
        session.StateChanged += (_, eventArgs) => StreamStateChanged?.Invoke(this, eventArgs);
        return session;
    }

    private static Uri BuildRtspUri(string ipAddress, int port)
    {
        return new UriBuilder("rtsp", ipAddress, port).Uri;
    }
}
