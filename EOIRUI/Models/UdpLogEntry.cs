namespace EOIRUI.Models;

public sealed record UdpLogEntry(
    DateTime Timestamp,
    string Direction,
    string EndPoint,
    string Message);
