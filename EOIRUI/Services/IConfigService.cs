using EOIRUI.Models;

namespace EOIRUI.Services;

public interface IConfigService
{
    string ConfigPath { get; }

    Task<AppConfig> LoadOrCreateAsync(CancellationToken cancellationToken = default);

    Task SaveAsync(AppConfig config, CancellationToken cancellationToken = default);
}
