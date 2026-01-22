using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace WinUIApp1.Services;

/// <summary>
/// 저장소 관리 서비스
/// - 녹화 용량 모니터링
/// - 최대 용량 초과 시 오래된 파일 삭제
/// - 날짜별 폴더 관리
/// </summary>
public class StorageService
{
    private readonly ConfigService _configService;

    public StorageService(ConfigService configService)
    {
        _configService = configService;
    }

    /// <summary>
    /// 현재 녹화 폴더의 총 용량 (바이트)
    /// </summary>
    public long GetCurrentStorageSize()
    {
        var recordingPath = _configService.Config.RecordingPath;
        if (!Directory.Exists(recordingPath))
            return 0;

        return GetDirectorySize(recordingPath);
    }

    /// <summary>
    /// 최대 용량 (바이트)
    /// </summary>
    public long GetMaxStorageSize()
    {
        return _configService.Config.MaxStorageMB * 1024 * 1024;
    }

    /// <summary>
    /// 사용률 (0~1)
    /// </summary>
    public double GetUsageRatio()
    {
        var current = GetCurrentStorageSize();
        var max = GetMaxStorageSize();
        return max > 0 ? (double)current / max : 0;
    }

    /// <summary>
    /// 용량 초과 시 오래된 파일/폴더 삭제
    /// </summary>
    public async Task CleanupIfNeededAsync()
    {
        var maxSize = GetMaxStorageSize();
        var currentSize = GetCurrentStorageSize();

        if (currentSize <= maxSize)
            return;

        var recordingPath = _configService.Config.RecordingPath;
        if (!Directory.Exists(recordingPath))
            return;

        // 오래된 파일부터 삭제
        var allFiles = GetAllRecordingFiles(recordingPath)
            .OrderBy(f => f.CreationTime)
            .ToList();

        foreach (var file in allFiles)
        {
            if (currentSize <= maxSize * 0.9) // 90%까지 줄이기
                break;

            try
            {
                var fileSize = file.Length;
                file.Delete();
                currentSize -= fileSize;

                // 빈 폴더 삭제
                CleanupEmptyFolders(recordingPath);
            }
            catch
            {
                // 삭제 실패 시 계속
            }
        }

        await Task.CompletedTask;
    }

    /// <summary>
    /// 특정 날짜 범위의 녹화 파일 목록
    /// </summary>
    public List<FileInfo> GetRecordingFiles(DateTime startDate, DateTime endDate)
    {
        var result = new List<FileInfo>();
        var recordingPath = _configService.Config.RecordingPath;

        if (!Directory.Exists(recordingPath))
            return result;

        for (var date = startDate.Date; date <= endDate.Date; date = date.AddDays(1))
        {
            var datePath = Path.Combine(
                recordingPath,
                date.ToString("yyyy"),
                date.ToString("MM"),
                date.ToString("dd"));

            if (!Directory.Exists(datePath))
                continue;

            var files = Directory.GetFiles(datePath, "*.*")
                .Where(f => f.EndsWith(".enc", StringComparison.OrdinalIgnoreCase) ||
                           f.EndsWith(".mp4", StringComparison.OrdinalIgnoreCase))
                .Select(f => new FileInfo(f));

            result.AddRange(files);
        }

        return result;
    }

    /// <summary>
    /// 녹화가 있는 날짜 목록
    /// </summary>
    public List<DateTime> GetRecordingDates()
    {
        var result = new List<DateTime>();
        var recordingPath = _configService.Config.RecordingPath;

        if (!Directory.Exists(recordingPath))
            return result;

        // YYYY/MM/DD 폴더 구조 탐색
        foreach (var yearDir in Directory.GetDirectories(recordingPath))
        {
            foreach (var monthDir in Directory.GetDirectories(yearDir))
            {
                foreach (var dayDir in Directory.GetDirectories(monthDir))
                {
                    // 해당 폴더에 파일이 있는지 확인
                    if (Directory.GetFiles(dayDir).Length > 0)
                    {
                        var year = Path.GetFileName(yearDir);
                        var month = Path.GetFileName(monthDir);
                        var day = Path.GetFileName(dayDir);

                        if (DateTime.TryParse($"{year}-{month}-{day}", out var date))
                        {
                            result.Add(date);
                        }
                    }
                }
            }
        }

        return result.OrderBy(d => d).ToList();
    }

    /// <summary>
    /// 디렉토리 크기 계산
    /// </summary>
    private long GetDirectorySize(string path)
    {
        long size = 0;

        try
        {
            var files = Directory.GetFiles(path, "*", SearchOption.AllDirectories);
            foreach (var file in files)
            {
                try
                {
                    size += new FileInfo(file).Length;
                }
                catch { }
            }
        }
        catch { }

        return size;
    }

    /// <summary>
    /// 모든 녹화 파일 가져오기
    /// </summary>
    private IEnumerable<FileInfo> GetAllRecordingFiles(string path)
    {
        var files = Directory.GetFiles(path, "*.*", SearchOption.AllDirectories)
            .Where(f => f.EndsWith(".enc", StringComparison.OrdinalIgnoreCase) ||
                       f.EndsWith(".mp4", StringComparison.OrdinalIgnoreCase))
            .Select(f => new FileInfo(f));

        return files;
    }

    /// <summary>
    /// 빈 폴더 삭제
    /// </summary>
    private void CleanupEmptyFolders(string path)
    {
        foreach (var dir in Directory.GetDirectories(path))
        {
            CleanupEmptyFolders(dir);

            if (Directory.GetFiles(dir).Length == 0 && Directory.GetDirectories(dir).Length == 0)
            {
                try
                {
                    Directory.Delete(dir);
                }
                catch { }
            }
        }
    }
}
