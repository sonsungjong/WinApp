using System.IO;
using System.Text.Json;
using EOIRUI.Models;

namespace EOIRUI.Services;

public sealed class ConfigService : IConfigService
{
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        WriteIndented = true
    };

    public ConfigService(string? configPath = null)
    {
        ConfigPath = configPath ?? Path.Combine(AppContext.BaseDirectory, "config.json");
    }

    public string ConfigPath { get; }

    public async Task<AppConfig> LoadOrCreateAsync(CancellationToken cancellationToken = default)
    {
        AppConfig config;

        if (File.Exists(ConfigPath))
        {
            await using var stream = File.OpenRead(ConfigPath);
            config = await JsonSerializer
                .DeserializeAsync<AppConfig>(stream, SerializerOptions, cancellationToken)
                .ConfigureAwait(false)
                ?? throw new InvalidDataException("config.json 내용을 읽을 수 없습니다.");
        }
        else
        {
            config = new AppConfig();
        }

        config.Validate();
        await SaveAsync(config, cancellationToken).ConfigureAwait(false);
        return config;
    }

    public async Task SaveAsync(AppConfig config, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(config);
        config.Validate();

        var directory = Path.GetDirectoryName(ConfigPath);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        await using var stream = File.Create(ConfigPath);
        await JsonSerializer
            .SerializeAsync(stream, config, SerializerOptions, cancellationToken)
            .ConfigureAwait(false);
    }
}
