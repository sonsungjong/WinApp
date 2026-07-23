using System.Net;
using EOIRUI.Models;

namespace EOIRUI.Services;

public sealed class CameraDataPacketEventArgs : EventArgs
{
    public CameraDataPacketEventArgs(
        CameraDataChannel channel,
        int localPort,
        byte[] data,
        IPEndPoint remoteEndPoint)
    {
        Channel = channel;
        LocalPort = localPort;
        Data = data;
        RemoteEndPoint = remoteEndPoint;
    }

    public CameraDataChannel Channel { get; }

    public int LocalPort { get; }

    public byte[] Data { get; }

    public IPEndPoint RemoteEndPoint { get; }
}

public sealed class CameraDataListenerFaultedEventArgs : EventArgs
{
    public CameraDataListenerFaultedEventArgs(
        CameraDataChannel channel,
        int localPort,
        Exception exception)
    {
        Channel = channel;
        LocalPort = localPort;
        Exception = exception;
    }

    public CameraDataChannel Channel { get; }

    public int LocalPort { get; }

    public Exception Exception { get; }
}
