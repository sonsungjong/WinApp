using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using WinUIApp1.Helpers;
using WinUIApp1.Models;

namespace WinUIApp1.Services;

/// <summary>
/// 녹화 서비스
/// - 실시간 스트림을 H.264로 인코딩
/// - 10분 단위 파일 분할
/// - AES-256 암호화 저장
/// - YYYY/MM/DD 폴더 구조
/// </summary>
public class RecordingService : IDisposable
{
    private readonly MediaFoundationService _mediaService;
    private readonly ConfigService _configService;
    private readonly StorageService _storageService;

    private FileStream? _currentFileStream;
    private string? _currentTempPath;
    private DateTime _segmentStartTime;
    private Timer? _segmentTimer;
    private bool _isRecording;
    private bool _isDisposed;
    private readonly object _lock = new();

    /// <summary>
    /// 녹화 상태
    /// </summary>
    public bool IsRecording => _isRecording;

    /// <summary>
    /// 현재 녹화 파일 경로
    /// </summary>
    public string? CurrentFilePath { get; private set; }

    /// <summary>
    /// 녹화 오류 이벤트
    /// </summary>
    public event EventHandler<Exception>? RecordingError;

    /// <summary>
    /// 녹화 파일 생성 이벤트
    /// </summary>
    public event EventHandler<RecordingFile>? RecordingFileCreated;

    public RecordingService(
        MediaFoundationService mediaService,
        ConfigService configService,
        StorageService storageService)
    {
        _mediaService = mediaService;
        _configService = configService;
        _storageService = storageService;
    }

    /// <summary>
    /// 녹화 시작 (상시 녹화)
    /// </summary>
    public void StartRecording(uint width = 1920, uint height = 1080, uint frameRate = 30)
    {
        if (_isRecording) return;

        lock (_lock)
        {
            _isRecording = true;
            StartNewSegment(width, height, frameRate);

            // 녹화 세그먼트 타이머 설정 (10분 단위)
            var segmentMinutes = _configService.Config.RecordingSegmentMinutes;
            _segmentTimer = new Timer(_ => RotateSegment(width, height, frameRate),
                null,
                TimeSpan.FromMinutes(segmentMinutes),
                TimeSpan.FromMinutes(segmentMinutes));
        }
    }

    /// <summary>
    /// 녹화 중지
    /// </summary>
    public async Task StopRecordingAsync()
    {
        if (!_isRecording) return;

        lock (_lock)
        {
            _isRecording = false;
            _segmentTimer?.Dispose();
            _segmentTimer = null;
        }

        await FinalizeCurrentSegmentAsync();
    }

    /// <summary>
    /// 새 녹화 세그먼트 시작
    /// </summary>
    private void StartNewSegment(uint width, uint height, uint frameRate)
    {
        _segmentStartTime = DateTime.Now;
        var config = _configService.Config;

        // 폴더 구조: YYYY/MM/DD
        var datePath = Path.Combine(
            config.RecordingPath,
            _segmentStartTime.ToString("yyyy"),
            _segmentStartTime.ToString("MM"),
            _segmentStartTime.ToString("dd"));

        Directory.CreateDirectory(datePath);

        // 임시 파일 (암호화 전)
        var fileName = $"{_segmentStartTime:yyyyMMdd_HHmmss}.mp4";
        _currentTempPath = Path.Combine(Path.GetTempPath(), $"rec_{Guid.NewGuid()}.mp4");
        CurrentFilePath = Path.Combine(datePath, $"{fileName}.enc");

        // Media Foundation 녹화 시작
        _mediaService.StartRecording(_currentTempPath, width, height, frameRate);
    }

    /// <summary>
    /// 녹화 세그먼트 회전 (10분 단위 분할)
    /// </summary>
    private async void RotateSegment(uint width, uint height, uint frameRate)
    {
        if (!_isRecording) return;

        await FinalizeCurrentSegmentAsync();

        lock (_lock)
        {
            if (_isRecording)
            {
                StartNewSegment(width, height, frameRate);
            }
        }
    }

    /// <summary>
    /// 현재 세그먼트 종료 및 암호화
    /// </summary>
    private async Task FinalizeCurrentSegmentAsync()
    {
        string? tempPath;
        string? finalPath;
        DateTime startTime;

        lock (_lock)
        {
            _mediaService.StopRecording();
            tempPath = _currentTempPath;
            finalPath = CurrentFilePath;
            startTime = _segmentStartTime;
            _currentTempPath = null;
            CurrentFilePath = null;
        }

        if (tempPath == null || finalPath == null) return;

        try
        {
            // 파일이 존재하는지 확인
            if (!File.Exists(tempPath))
            {
                return;
            }

            // AES-256 암호화
            var encryptionKey = _configService.Config.EncryptionKey;
            if (!string.IsNullOrEmpty(encryptionKey))
            {
                await EncryptionHelper.EncryptFileAsync(tempPath, finalPath, encryptionKey);
                File.Delete(tempPath); // 임시 파일 삭제
            }
            else
            {
                // 암호화 키가 없으면 그냥 이동
                File.Move(tempPath, finalPath.Replace(".enc", ".mp4"));
                finalPath = finalPath.Replace(".enc", ".mp4");
            }

            // 녹화 파일 메타데이터 생성
            var recordingFile = new RecordingFile
            {
                FilePath = finalPath,
                StartTime = startTime.ToUniversalTime(),
                EndTime = DateTime.Now.ToUniversalTime(),
                FileSize = new FileInfo(finalPath).Length
            };

            RecordingFileCreated?.Invoke(this, recordingFile);

            // 용량 확인 및 정리
            await _storageService.CleanupIfNeededAsync();
        }
        catch (Exception ex)
        {
            RecordingError?.Invoke(this, ex);
        }
    }

    public void Dispose()
    {
        if (_isDisposed) return;
        _isDisposed = true;

        _segmentTimer?.Dispose();
        _segmentTimer = null;

        if (_isRecording)
        {
            _isRecording = false;
            _mediaService.StopRecording();

            // 임시 파일 정리
            if (_currentTempPath != null && File.Exists(_currentTempPath))
            {
                try { File.Delete(_currentTempPath); } catch { }
            }
        }
    }
}
