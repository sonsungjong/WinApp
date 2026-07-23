using System.IO;
using System.Net;
using System.Text.Json.Serialization;

namespace EOIRUI.Models;

public sealed class AppConfig
{
    [JsonPropertyName("eo_cv_ip")]
    public string EoCvIp { get; set; } = "127.0.0.1";

    [JsonPropertyName("eo_cv_port")]
    public int EoCvPort { get; set; } = 55555;

    [JsonPropertyName("ir_cv_ip")]
    public string IrCvIp { get; set; } = "127.0.0.1";

    [JsonPropertyName("ir_cv_port")]
    public int IrCvPort { get; set; } = 55556;

    [JsonPropertyName("eo_data_ip")]
    public string EoDataIp { get; set; } = "127.0.0.1";

    [JsonPropertyName("eo_data_port")]
    public int EoDataPort { get; set; } = 55557;

    [JsonPropertyName("ir_data_ip")]
    public string IrDataIp { get; set; } = "127.0.0.1";

    [JsonPropertyName("ir_data_port")]
    public int IrDataPort { get; set; } = 55558;

    public void Validate()
    {
        ValidateIp(EoCvIp, nameof(EoCvIp));
        ValidatePort(EoCvPort, nameof(EoCvPort));
        ValidateIp(IrCvIp, nameof(IrCvIp));
        ValidatePort(IrCvPort, nameof(IrCvPort));
        ValidateIp(EoDataIp, nameof(EoDataIp));
        ValidatePort(EoDataPort, nameof(EoDataPort));
        ValidateIp(IrDataIp, nameof(IrDataIp));
        ValidatePort(IrDataPort, nameof(IrDataPort));
    }

    private static void ValidateIp(string value, string propertyName)
    {
        if (!IPAddress.TryParse(value, out _))
        {
            throw new InvalidDataException($"{propertyName}에 올바른 IP 주소를 입력해야 합니다.");
        }
    }

    private static void ValidatePort(int value, string propertyName)
    {
        if (value is < 1 or > 65535)
        {
            throw new InvalidDataException($"{propertyName}은 1에서 65535 사이여야 합니다.");
        }
    }
}
