using EOIRUI.Models;

namespace EOIRUI.Services;

public enum RtspStreamState
{
    Stopped,
    Connecting,
    Buffering,
    Playing,
    Reconnecting,
    Faulted
}

public sealed class RtspStreamStateChangedEventArgs : EventArgs
{
    public RtspStreamStateChangedEventArgs(
        CameraKind camera,
        RtspStreamState state,
        string message)
    {
        Camera = camera;
        State = state;
        Message = message;
    }

    public CameraKind Camera { get; }

    public RtspStreamState State { get; }

    public string Message { get; }
}
