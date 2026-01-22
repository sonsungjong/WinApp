using System;
using System.IO;

namespace WinUIApp1.Models;

/// <summary>
/// 녹화 파일 메타데이터
/// </summary>
public class RecordingFile
{
    /// <summary>
    /// 암호화된 파일의 전체 경로
    /// </summary>
    public string FilePath { get; set; } = "";

    /// <summary>
    /// 녹화 시작 시간 (UTC)
    /// </summary>
    public DateTime StartTime { get; set; }

    /// <summary>
    /// 녹화 종료 시간 (UTC)
    /// </summary>
    public DateTime EndTime { get; set; }

    /// <summary>
    /// 파일 크기 (바이트)
    /// </summary>
    public long FileSize { get; set; }

    /// <summary>
    /// 녹화 시간 (로컬 시간 기준 파일명)
    /// 형식: yyyyMMdd_HHmmss
    /// </summary>
    public string FileName => Path.GetFileNameWithoutExtension(FilePath);

    /// <summary>
    /// 해당 시간이 이 녹화 파일 범위 내에 있는지 확인
    /// </summary>
    public bool ContainsTime(DateTime time)
    {
        return time >= StartTime && time <= EndTime;
    }
}
