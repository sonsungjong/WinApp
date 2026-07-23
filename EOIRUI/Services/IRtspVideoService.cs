using LibVLCSharp.Shared;

namespace EOIRUI.Services;

public interface IRtspVideoService : IDisposable
{
    event EventHandler<RtspStreamStateChangedEventArgs>? StreamStateChanged;

    MediaPlayer EoMediaPlayer { get; }

    MediaPlayer IrMediaPlayer { get; }

    Task StartAsync(CancellationToken cancellationToken = default);

    Task StopAsync();
}
