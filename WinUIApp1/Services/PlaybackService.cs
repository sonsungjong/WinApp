using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using WinUIApp1.Helpers;
using WinUIApp1.Models;

namespace WinUIApp1.Services;

/// <summary>
/// 재생 서비스
/// - 암호화된 녹화 파일 복호화
/// - 선택 시간 기준 ±10분 파일 로드
/// - 복호화 파일 임시 저장 및 자동 삭제
/// </summary>
public class PlaybackService : IDisposable
{
    private readonly MediaFoundationService _mediaService;
    private readonly ConfigService _configService;
    private readonly string _tempDir;
    private List<string> _decryptedFiles = new();
    private bool _isPlaying;
    private bool _isPaused;
    private bool _isDisposed;

    /// <summary>
    /// 재생 상태
    /// </summary>
    public bool IsPlaying => _isPlaying;

    /// <summary>
    /// 일시정지 상태
    /// </summary>
    public bool IsPaused => _isPaused;

    /// <summary>
    /// 현재 재생 시간
    /// </summary>
    public TimeSpan CurrentPosition { get; private set; }

    /// <summary>
    /// 총 재생 시간
    /// </summary>
    public TimeSpan Duration { get; private set; }

    /// <summary>
    /// 재생 종료 이벤트
    /// </summary>
    public event EventHandler? PlaybackEnded;

    /// <summary>
    /// 재생 오류 이벤트
    /// </summary>
    public event EventHandler<Exception>? PlaybackError;

    /// <summary>
    /// 재생 준비 완료 이벤트
    /// </summary>
    public event EventHandler? PlaybackReady;

    public PlaybackService(MediaFoundationService mediaService, ConfigService configService)
    {
        _mediaService = mediaService;
        _configService = configService;
        _tempDir = Path.Combine(Path.GetTempPath(), "WinUIApp1_Playback");
        Directory.CreateDirectory(_tempDir);

        _mediaService.StreamEnded += (s, e) => PlaybackEnded?.Invoke(this, EventArgs.Empty);
        _mediaService.StreamError += (s, e) => PlaybackError?.Invoke(this, e);
    }

    /// <summary>
    /// 녹화 파일 불러오기 (선택 시간 기준 ±10분)
    /// </summary>
    /// <param name="targetTime">대상 시간</param>
    public async Task LoadRecordingsAsync(DateTime targetTime)
    {
        // 기존 복호화 파일 삭제
        await CleanupDecryptedFilesAsync();

        var config = _configService.Config;
        var encryptionKey = config.EncryptionKey;
        var recordingPath = config.RecordingPath;

        // 대상 시간 기준 ±10분 범위
        var startTime = targetTime.AddMinutes(-10);
        var endTime = targetTime.AddMinutes(10);

        // 녹화 파일 검색 (YYYY/MM/DD 폴더 구조)
        var files = FindRecordingFiles(recordingPath, startTime, endTime);

        if (files.Count == 0)
        {
            PlaybackError?.Invoke(this, new FileNotFoundException("해당 시간대의 녹화 파일이 없습니다."));
            return;
        }

        // 파일 복호화
        foreach (var file in files.OrderBy(f => f.StartTime))
        {
            try
            {
                var decryptedPath = Path.Combine(_tempDir, $"play_{Path.GetFileNameWithoutExtension(file.FilePath)}.mp4");

                if (file.FilePath.EndsWith(".enc", StringComparison.OrdinalIgnoreCase) && !string.IsNullOrEmpty(encryptionKey))
                {
                    await EncryptionHelper.DecryptFileAsync(file.FilePath, decryptedPath, encryptionKey);
                }
                else
                {
                    File.Copy(file.FilePath, decryptedPath, true);
                }

                _decryptedFiles.Add(decryptedPath);
            }
            catch (Exception ex)
            {
                PlaybackError?.Invoke(this, ex);
            }
        }

        // 총 재생 시간 계산
        Duration = TimeSpan.FromMinutes(20); // ±10분

        PlaybackReady?.Invoke(this, EventArgs.Empty);
    }

    /// <summary>
    /// 녹화 파일 검색
    /// </summary>
    private List<RecordingFile> FindRecordingFiles(string recordingPath, DateTime startTime, DateTime endTime)
    {
        var result = new List<RecordingFile>();

        if (!Directory.Exists(recordingPath))
            return result;

        // 관련 날짜 폴더 검색
        for (var date = startTime.Date; date <= endTime.Date; date = date.AddDays(1))
        {
            var datePath = Path.Combine(
                recordingPath,
                date.ToString("yyyy"),
                date.ToString("MM"),
                date.ToString("dd"));

            if (!Directory.Exists(datePath))
                continue;

            // .enc 또는 .mp4 파일 검색
            var files = Directory.GetFiles(datePath, "*.*")
                .Where(f => f.EndsWith(".enc", StringComparison.OrdinalIgnoreCase) ||
                           f.EndsWith(".mp4", StringComparison.OrdinalIgnoreCase));

            foreach (var file in files)
            {
                var fileName = Path.GetFileNameWithoutExtension(file).Replace(".enc", "");
                // 파일명 형식: yyyyMMdd_HHmmss
                if (DateTime.TryParseExact(fileName, "yyyyMMdd_HHmmss",
                    null, System.Globalization.DateTimeStyles.None, out var fileTime))
                {
                    // 10분 세그먼트로 가정
                    var fileEndTime = fileTime.AddMinutes(10);

                    // 시간 범위 체크
                    if (fileTime <= endTime && fileEndTime >= startTime)
                    {
                        result.Add(new RecordingFile
                        {
                            FilePath = file,
                            StartTime = fileTime.ToUniversalTime(),
                            EndTime = fileEndTime.ToUniversalTime(),
                            FileSize = new FileInfo(file).Length
                        });
                    }
                }
            }
        }

        return result;
    }

    /// <summary>
    /// 재생 시작
    /// </summary>
    public async Task PlayAsync()
    {
        if (_decryptedFiles.Count == 0)
        {
            PlaybackError?.Invoke(this, new InvalidOperationException("재생할 파일이 없습니다. 먼저 불러오기를 하세요."));
            return;
        }

        if (_isPaused)
        {
            _isPaused = false;
            _isPlaying = true;
            // TODO: Resume playback
            return;
        }

        // 첫 번째 파일부터 재생
        var firstFile = _decryptedFiles.First();
        await _mediaService.StartPlaybackAsync(firstFile);
        _isPlaying = true;
        _isPaused = false;
    }

    /// <summary>
    /// 일시정지
    /// </summary>
    public void Pause()
    {
        if (!_isPlaying) return;
        _isPaused = true;
        _isPlaying = false;
        // TODO: Pause playback in MediaFoundationService
    }

    /// <summary>
    /// 재생 중지
    /// </summary>
    public async Task StopAsync()
    {
        if (!_isPlaying && !_isPaused) return;

        await _mediaService.StopStreamingAsync();
        _isPlaying = false;
        _isPaused = false;
        CurrentPosition = TimeSpan.Zero;
    }

    /// <summary>
    /// 특정 위치로 이동
    /// </summary>
    public async Task SeekAsync(TimeSpan position)
    {
        // TODO: Implement seeking
        CurrentPosition = position;
    }

    /// <summary>
    /// 복호화 파일 삭제
    /// </summary>
    public async Task CleanupDecryptedFilesAsync()
    {
        if (_isPlaying || _isPaused)
        {
            await StopAsync();
        }

        foreach (var file in _decryptedFiles)
        {
            try
            {
                if (File.Exists(file))
                    File.Delete(file);
            }
            catch { }
        }
        _decryptedFiles.Clear();
    }

    public void Dispose()
    {
        if (_isDisposed) return;
        _isDisposed = true;

        _ = CleanupDecryptedFilesAsync();

        // 임시 폴더 정리
        try
        {
            if (Directory.Exists(_tempDir))
                Directory.Delete(_tempDir, true);
        }
        catch { }
    }
}
