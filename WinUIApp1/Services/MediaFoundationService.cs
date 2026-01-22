using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using WinUIApp1.Native;

namespace WinUIApp1.Services;

/// <summary>
/// Windows Media Foundation 스트리밍/디코딩/인코딩 서비스
/// - RTSP URL에서 비디오 스트림 읽기
/// - DXVA2 GPU 하드웨어 가속
/// - H.264 인코딩으로 파일 저장
/// </summary>
public class MediaFoundationService : IDisposable
{
    private IMFSourceReader? _sourceReader;
    private IMFSinkWriter? _sinkWriter;
    private bool _isInitialized;
    private bool _isDisposed;
    private CancellationTokenSource? _readCts;
    private uint _videoStreamIndex;

    /// <summary>
    /// 스트리밍 상태
    /// </summary>
    public bool IsStreaming { get; private set; }

    /// <summary>
    /// 녹화 상태
    /// </summary>
    public bool IsRecording { get; private set; }

    /// <summary>
    /// 영상 프레임 수신 이벤트
    /// </summary>
    public event EventHandler<VideoFrameEventArgs>? FrameReceived;

    /// <summary>
    /// 스트림 오류 이벤트
    /// </summary>
    public event EventHandler<Exception>? StreamError;

    /// <summary>
    /// 스트림 종료 이벤트
    /// </summary>
    public event EventHandler? StreamEnded;

    /// <summary>
    /// Media Foundation 초기화
    /// </summary>
    public void Initialize()
    {
        if (_isInitialized) return;

        MediaFoundationInterop.MFStartup(MediaFoundationInterop.MF_VERSION);
        _isInitialized = true;
    }

    /// <summary>
    /// RTSP URL에서 스트리밍 시작
    /// </summary>
    public async Task StartStreamingAsync(string rtspUrl)
    {
        if (!_isInitialized)
            throw new InvalidOperationException("Media Foundation not initialized");

        if (IsStreaming)
            await StopStreamingAsync();

        try
        {
            // Source Reader 속성 설정 (GPU 가속 활성화)
            MediaFoundationInterop.MFCreateAttributes(out var attributes, 3);
            attributes.SetUINT32(MediaFoundationInterop.MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
            attributes.SetUINT32(MediaFoundationInterop.MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);
            attributes.SetUINT32(MediaFoundationInterop.MF_LOW_LATENCY, 1);

            // RTSP URL에서 Source Reader 생성
            MediaFoundationInterop.MFCreateSourceReaderFromURL(rtspUrl, attributes, out _sourceReader);

            // 비디오 스트림 선택
            _sourceReader.SetStreamSelection(0xFFFFFFFC, true); // MF_SOURCE_READER_FIRST_VIDEO_STREAM

            // 출력 형식을 NV12 또는 RGB32로 설정 (디코딩된 프레임)
            ConfigureOutputType();

            IsStreaming = true;
            _readCts = new CancellationTokenSource();

            // 프레임 읽기 루프 시작
            _ = Task.Run(() => ReadFrameLoopAsync(_readCts.Token));

            Marshal.ReleaseComObject(attributes);
        }
        catch (Exception ex)
        {
            StreamError?.Invoke(this, ex);
            throw;
        }
    }

    /// <summary>
    /// 로컬 파일에서 재생 시작
    /// </summary>
    public async Task StartPlaybackAsync(string filePath)
    {
        if (!_isInitialized)
            throw new InvalidOperationException("Media Foundation not initialized");

        if (IsStreaming)
            await StopStreamingAsync();

        try
        {
            MediaFoundationInterop.MFCreateAttributes(out var attributes, 2);
            attributes.SetUINT32(MediaFoundationInterop.MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
            attributes.SetUINT32(MediaFoundationInterop.MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);

            MediaFoundationInterop.MFCreateSourceReaderFromURL(filePath, attributes, out _sourceReader);
            _sourceReader.SetStreamSelection(0xFFFFFFFC, true);

            ConfigureOutputType();

            IsStreaming = true;
            _readCts = new CancellationTokenSource();
            _ = Task.Run(() => ReadFrameLoopAsync(_readCts.Token));

            Marshal.ReleaseComObject(attributes);
        }
        catch (Exception ex)
        {
            StreamError?.Invoke(this, ex);
            throw;
        }
    }

    /// <summary>
    /// 스트리밍 중지
    /// </summary>
    public async Task StopStreamingAsync()
    {
        IsStreaming = false;
        _readCts?.Cancel();

        await Task.Delay(100); // 프레임 읽기 루프 종료 대기

        if (_sourceReader != null)
        {
            Marshal.ReleaseComObject(_sourceReader);
            _sourceReader = null;
        }
    }

    /// <summary>
    /// 녹화 시작
    /// </summary>
    public void StartRecording(string outputPath, uint width, uint height, uint frameRate)
    {
        if (IsRecording) return;

        try
        {
            MediaFoundationInterop.MFCreateAttributes(out var attributes, 1);
            attributes.SetUINT32(MediaFoundationInterop.MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);

            MediaFoundationInterop.MFCreateSinkWriterFromURL(outputPath, IntPtr.Zero, attributes, out _sinkWriter);

            // 출력 미디어 타입 설정 (H.264)
            MediaFoundationInterop.MFCreateMediaType(out var outputType);
            outputType.SetGUID(MediaFoundationInterop.MF_MT_MAJOR_TYPE, MediaFoundationInterop.MFMediaType_Video);
            outputType.SetGUID(MediaFoundationInterop.MF_MT_SUBTYPE, MediaFoundationInterop.MFVideoFormat_H264);
            outputType.SetUINT64(MediaFoundationInterop.MF_MT_FRAME_SIZE, MediaFoundationInterop.PackSize(width, height));
            outputType.SetUINT64(MediaFoundationInterop.MF_MT_FRAME_RATE, MediaFoundationInterop.PackRatio(frameRate, 1));
            outputType.SetUINT32(MediaFoundationInterop.MF_MT_AVG_BITRATE, 5_000_000); // 5 Mbps
            outputType.SetUINT32(MediaFoundationInterop.MF_MT_INTERLACE_MODE, 2); // Progressive

            _sinkWriter.AddStream(outputType, out _videoStreamIndex);

            // 입력 미디어 타입 설정 (NV12)
            MediaFoundationInterop.MFCreateMediaType(out var inputType);
            inputType.SetGUID(MediaFoundationInterop.MF_MT_MAJOR_TYPE, MediaFoundationInterop.MFMediaType_Video);
            inputType.SetGUID(MediaFoundationInterop.MF_MT_SUBTYPE, MediaFoundationInterop.MFVideoFormat_NV12);
            inputType.SetUINT64(MediaFoundationInterop.MF_MT_FRAME_SIZE, MediaFoundationInterop.PackSize(width, height));
            inputType.SetUINT64(MediaFoundationInterop.MF_MT_FRAME_RATE, MediaFoundationInterop.PackRatio(frameRate, 1));

            _sinkWriter.SetInputMediaType(_videoStreamIndex, inputType, null);
            _sinkWriter.BeginWriting();

            IsRecording = true;

            Marshal.ReleaseComObject(attributes);
            Marshal.ReleaseComObject(outputType);
            Marshal.ReleaseComObject(inputType);
        }
        catch (Exception ex)
        {
            StreamError?.Invoke(this, ex);
            throw;
        }
    }

    /// <summary>
    /// 녹화 중지
    /// </summary>
    public void StopRecording()
    {
        if (!IsRecording || _sinkWriter == null) return;

        try
        {
            _sinkWriter.Finalize_();
            Marshal.ReleaseComObject(_sinkWriter);
            _sinkWriter = null;
            IsRecording = false;
        }
        catch (Exception ex)
        {
            StreamError?.Invoke(this, ex);
        }
    }

    /// <summary>
    /// 녹화에 프레임 기록
    /// </summary>
    public void WriteFrame(IMFSample sample)
    {
        if (!IsRecording || _sinkWriter == null) return;

        try
        {
            _sinkWriter.WriteSample(_videoStreamIndex, sample);
        }
        catch (Exception ex)
        {
            StreamError?.Invoke(this, ex);
        }
    }

    /// <summary>
    /// 출력 미디어 타입 설정
    /// </summary>
    private void ConfigureOutputType()
    {
        if (_sourceReader == null) return;

        // NV12 형식으로 출력 요청 (GPU 디코딩 최적)
        MediaFoundationInterop.MFCreateMediaType(out var outputType);
        outputType.SetGUID(MediaFoundationInterop.MF_MT_MAJOR_TYPE, MediaFoundationInterop.MFMediaType_Video);
        outputType.SetGUID(MediaFoundationInterop.MF_MT_SUBTYPE, MediaFoundationInterop.MFVideoFormat_NV12);

        try
        {
            _sourceReader.SetCurrentMediaType(0xFFFFFFFC, IntPtr.Zero, outputType);
        }
        catch
        {
            // NV12 실패 시 RGB32로 시도
            outputType.SetGUID(MediaFoundationInterop.MF_MT_SUBTYPE, MediaFoundationInterop.MFVideoFormat_RGB32);
            _sourceReader.SetCurrentMediaType(0xFFFFFFFC, IntPtr.Zero, outputType);
        }

        Marshal.ReleaseComObject(outputType);
    }

    /// <summary>
    /// 프레임 읽기 루프
    /// </summary>
    private async Task ReadFrameLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested && IsStreaming && _sourceReader != null)
        {
            try
            {
                _sourceReader.ReadSample(
                    0xFFFFFFFC, // MF_SOURCE_READER_FIRST_VIDEO_STREAM
                    0,
                    out uint actualStreamIndex,
                    out uint streamFlags,
                    out long timestamp,
                    out IMFSample? sample);

                // 스트림 종료 체크
                if ((streamFlags & MFSourceReaderStreamFlags.MF_SOURCE_READERF_ENDOFSTREAM) != 0)
                {
                    StreamEnded?.Invoke(this, EventArgs.Empty);
                    break;
                }

                // 오류 체크
                if ((streamFlags & MFSourceReaderStreamFlags.MF_SOURCE_READERF_ERROR) != 0)
                {
                    StreamError?.Invoke(this, new Exception("Stream read error"));
                    break;
                }

                if (sample != null)
                {
                    // 프레임 이벤트 발생
                    FrameReceived?.Invoke(this, new VideoFrameEventArgs(sample, timestamp));

                    // 녹화 중이면 프레임 기록
                    if (IsRecording)
                    {
                        WriteFrame(sample);
                    }

                    Marshal.ReleaseComObject(sample);
                }

                // CPU 과부하 방지
                await Task.Yield();
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception ex)
            {
                StreamError?.Invoke(this, ex);
                // 일시적 오류일 수 있으므로 계속 시도
                await Task.Delay(100, cancellationToken);
            }
        }
    }

    public void Dispose()
    {
        if (_isDisposed) return;
        _isDisposed = true;

        _readCts?.Cancel();
        StopRecording();

        if (_sourceReader != null)
        {
            Marshal.ReleaseComObject(_sourceReader);
            _sourceReader = null;
        }

        if (_isInitialized)
        {
            MediaFoundationInterop.MFShutdown();
            _isInitialized = false;
        }
    }
}

/// <summary>
/// 비디오 프레임 이벤트 인자
/// </summary>
public class VideoFrameEventArgs : EventArgs
{
    public IMFSample Sample { get; }
    public long Timestamp { get; }

    public VideoFrameEventArgs(IMFSample sample, long timestamp)
    {
        Sample = sample;
        Timestamp = timestamp;
    }
}
