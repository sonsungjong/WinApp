using System.Text.Json.Serialization;

namespace WinUIApp1.Models;

/// <summary>
/// 앱 설정 모델 - config.json에서 로드
/// </summary>
public class AppConfig
{
    [JsonPropertyName("cameraIp")]
    public string CameraIp { get; set; } = "192.168.1.100";

    [JsonPropertyName("cameraPort")]
    public int CameraPort { get; set; } = 554;

    [JsonPropertyName("tcpControlPort")]
    public int TcpControlPort { get; set; } = 8000;

    [JsonPropertyName("rtspUrl")]
    public string RtspUrl { get; set; } = "rtsp://192.168.1.100:554/stream";

    [JsonPropertyName("recordingPath")]
    public string RecordingPath { get; set; } = @"C:\Recordings";

    [JsonPropertyName("maxStorageMB")]
    public long MaxStorageMB { get; set; } = 10240; // 10GB

    [JsonPropertyName("encryptionKey")]
    public string EncryptionKey { get; set; } = "";

    [JsonPropertyName("recordingSegmentMinutes")]
    public int RecordingSegmentMinutes { get; set; } = 10;
}
