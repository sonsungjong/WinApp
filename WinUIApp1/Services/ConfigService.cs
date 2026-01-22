using System;
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;
using WinUIApp1.Models;

namespace WinUIApp1.Services;

/// <summary>
/// 설정 파일 로드/저장 서비스
/// </summary>
public class ConfigService
{
    private readonly string _configPath;
    private AppConfig? _config;

    public AppConfig Config => _config ?? throw new InvalidOperationException("Config not loaded");

    public ConfigService()
    {
        // 앱 실행 폴더의 config.json 경로
        var appDir = AppContext.BaseDirectory;
        _configPath = Path.Combine(appDir, "config.json");
    }

    /// <summary>
    /// config.json 파일을 로드합니다
    /// </summary>
    public async Task<AppConfig> LoadAsync()
    {
        try
        {
            if (File.Exists(_configPath))
            {
                var json = await File.ReadAllTextAsync(_configPath);
                _config = JsonSerializer.Deserialize<AppConfig>(json) ?? new AppConfig();
            }
            else
            {
                _config = new AppConfig();
                await SaveAsync(); // 기본 설정 파일 생성
            }
        }
        catch (Exception)
        {
            _config = new AppConfig();
        }

        return _config;
    }

    /// <summary>
    /// 현재 설정을 config.json에 저장합니다
    /// </summary>
    public async Task SaveAsync()
    {
        if (_config == null) return;

        var options = new JsonSerializerOptions
        {
            WriteIndented = true
        };

        var json = JsonSerializer.Serialize(_config, options);
        await File.WriteAllTextAsync(_configPath, json);
    }

    /// <summary>
    /// 설정값 업데이트
    /// </summary>
    public async Task UpdateAsync(Action<AppConfig> update)
    {
        if (_config == null) await LoadAsync();
        update(_config!);
        await SaveAsync();
    }
}
