using System.Net;

namespace EOIRUI.Services;

public sealed class UdpDatagramReceivedEventArgs : EventArgs
{
    public UdpDatagramReceivedEventArgs(byte[] data, IPEndPoint remoteEndPoint)
    {
        Data = data;
        RemoteEndPoint = remoteEndPoint;
    }

    public byte[] Data { get; }

    public IPEndPoint RemoteEndPoint { get; }
}
