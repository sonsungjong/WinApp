using LibVLCSharp.Shared;

namespace EOIRUI.ViewModels;

public sealed class CameraFeedViewModel : ViewModelBase
{
    private long _videoPacketCount;
    private long _videoByteCount;
    private long _dataPacketCount;
    private long _dataByteCount;
    private string _videoStatus = "영상 패킷 대기 중";
    private string _dataStatus = "추가 데이터 대기 중";

    public CameraFeedViewModel(
        string name,
        MediaPlayer mediaPlayer,
        string videoIp,
        int videoPort,
        string dataIp,
        int dataPort)
    {
        Name = name;
        MediaPlayer = mediaPlayer;
        VideoIp = videoIp;
        VideoPort = videoPort;
        DataIp = dataIp;
        DataPort = dataPort;
    }

    public string Name { get; }

    public MediaPlayer MediaPlayer { get; }

    public string VideoIp { get; }

    public int VideoPort { get; }

    public string DataIp { get; }

    public int DataPort { get; }

    public long VideoPacketCount
    {
        get => _videoPacketCount;
        private set => SetProperty(ref _videoPacketCount, value);
    }

    public long VideoByteCount
    {
        get => _videoByteCount;
        private set => SetProperty(ref _videoByteCount, value);
    }

    public long DataPacketCount
    {
        get => _dataPacketCount;
        private set => SetProperty(ref _dataPacketCount, value);
    }

    public long DataByteCount
    {
        get => _dataByteCount;
        private set => SetProperty(ref _dataByteCount, value);
    }

    public string VideoStatus
    {
        get => _videoStatus;
        private set => SetProperty(ref _videoStatus, value);
    }

    public string DataStatus
    {
        get => _dataStatus;
        private set => SetProperty(ref _dataStatus, value);
    }

    public void UpdateVideoStatistics(long packetCount, long byteCount)
    {
        VideoPacketCount = packetCount;
        VideoByteCount = byteCount;
        VideoStatus = $"수신 중 · {VideoPacketCount:N0} packets · {VideoByteCount:N0} bytes";
    }

    public void UpdateDataStatistics(long packetCount, long byteCount)
    {
        DataPacketCount = packetCount;
        DataByteCount = byteCount;
        DataStatus = $"수신 중 · {DataPacketCount:N0} packets · {DataByteCount:N0} bytes";
    }

    public void SetVideoFault(string message) => VideoStatus = message;

    public void SetVideoStatus(string message) => VideoStatus = message;

    public void SetDataFault(string message) => DataStatus = message;
}
