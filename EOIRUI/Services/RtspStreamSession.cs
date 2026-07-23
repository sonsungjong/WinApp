using EOIRUI.Models;
using LibVLCSharp.Shared;

namespace EOIRUI.Services;

internal sealed class RtspStreamSession : IDisposable
{
    private readonly object _lifecycleLock = new();
    private readonly LibVLC _libVlc;
    private CancellationTokenSource? _runCancellation;
    private Task? _runTask;
    private TaskCompletionSource? _attemptEnded;
    private bool _isDisposed;

    public RtspStreamSession(LibVLC libVlc, CameraKind camera, Uri streamUri)
    {
        _libVlc = libVlc;
        Camera = camera;
        StreamUri = streamUri;
        MediaPlayer = new MediaPlayer(libVlc)
        {
            EnableHardwareDecoding = true
        };

        MediaPlayer.Opening += OnOpening;
        MediaPlayer.Buffering += OnBuffering;
        MediaPlayer.Playing += OnPlaying;
        MediaPlayer.EndReached += OnAttemptEnded;
        MediaPlayer.EncounteredError += OnEncounteredError;
        MediaPlayer.Stopped += OnAttemptEnded;
    }

    public event EventHandler<RtspStreamStateChangedEventArgs>? StateChanged;

    public CameraKind Camera { get; }

    public Uri StreamUri { get; }

    public MediaPlayer MediaPlayer { get; }

    public Task StartAsync(CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);

        lock (_lifecycleLock)
        {
            if (_runTask is not null)
            {
                return Task.CompletedTask;
            }

            _runCancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            _runTask = RunAsync(_runCancellation.Token);
        }

        return Task.CompletedTask;
    }

    public async Task StopAsync()
    {
        CancellationTokenSource? cancellation;
        Task? runTask;

        lock (_lifecycleLock)
        {
            cancellation = _runCancellation;
            runTask = _runTask;
            _runCancellation = null;
            _runTask = null;
        }

        cancellation?.Cancel();
        MediaPlayer.Stop();

        if (runTask is not null)
        {
            try
            {
                await runTask.ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                // 정상적인 RTSP 세션 종료입니다.
            }
        }

        cancellation?.Dispose();
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        StopAsync().GetAwaiter().GetResult();
        MediaPlayer.Dispose();
    }

    private async Task RunAsync(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var attemptEnded = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
                Interlocked.Exchange(ref _attemptEnded, attemptEnded);

                RaiseState(RtspStreamState.Connecting, $"RTSP 연결 중 · {StreamUri}");

                using var media = new Media(_libVlc, StreamUri);
                media.AddOption(":no-audio");
                media.AddOption(":network-caching=200");

                if (!MediaPlayer.Play(media))
                {
                    RaiseState(RtspStreamState.Faulted, $"RTSP 재생 시작 실패 · {StreamUri}");
                }
                else
                {
                    await attemptEnded.Task.WaitAsync(cancellationToken).ConfigureAwait(false);
                }

                if (cancellationToken.IsCancellationRequested)
                {
                    break;
                }

                RaiseState(RtspStreamState.Reconnecting, "RTSP 연결 끊김 · 2초 후 재연결");
                MediaPlayer.Stop();
                await Task.Delay(TimeSpan.FromSeconds(2), cancellationToken).ConfigureAwait(false);
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            // 정상적인 종료입니다.
        }
        finally
        {
            Interlocked.Exchange(ref _attemptEnded, null);
            RaiseState(RtspStreamState.Stopped, "RTSP 수신 중지됨");
        }
    }

    private void OnOpening(object? sender, EventArgs e)
    {
        RaiseState(RtspStreamState.Connecting, $"RTSP 연결 중 · {StreamUri}");
    }

    private void OnBuffering(object? sender, MediaPlayerBufferingEventArgs e)
    {
        RaiseState(RtspStreamState.Buffering, $"RTSP 버퍼링 · {e.Cache:0}%");
    }

    private void OnPlaying(object? sender, EventArgs e)
    {
        RaiseState(RtspStreamState.Playing, $"RTSP 영상 수신 중 · {StreamUri}");
    }

    private void OnEncounteredError(object? sender, EventArgs e)
    {
        RaiseState(RtspStreamState.Faulted, $"RTSP 수신 오류 · {StreamUri}");
        Volatile.Read(ref _attemptEnded)?.TrySetResult();
    }

    private void OnAttemptEnded(object? sender, EventArgs e)
    {
        Volatile.Read(ref _attemptEnded)?.TrySetResult();
    }

    private void RaiseState(RtspStreamState state, string message)
    {
        StateChanged?.Invoke(this, new RtspStreamStateChangedEventArgs(Camera, state, message));
    }
}
