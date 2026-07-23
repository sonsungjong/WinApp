using System.Net;
using EOIRUI.Models;

namespace EOIRUI.Services;

public sealed class CameraDataPacketEventArgs : EventArgs
{
    public CameraDataPacketEventArgs(
        CameraKind camera,
        int localPort,
        byte[] data,
        IPEndPoint remoteEndPoint)
    {
        Camera = camera;
        LocalPort = localPort;
        Data = data;
        RemoteEndPoint = remoteEndPoint;
    }

    public CameraKind Camera { get; }

    public int LocalPort { get; }

    public byte[] Data { get; }

    public IPEndPoint RemoteEndPoint { get; }
}

public sealed class CameraDataListenerFaultedEventArgs : EventArgs
{
    public CameraDataListenerFaultedEventArgs(
        CameraKind camera,
        int localPort,
        Exception exception)
    {
        Camera = camera;
        LocalPort = localPort;
        Exception = exception;
    }

    public CameraKind Camera { get; }

    public int LocalPort { get; }

    public Exception Exception { get; }
}
