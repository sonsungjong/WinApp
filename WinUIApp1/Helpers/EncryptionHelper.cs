using System;
using System.IO;
using System.Security.Cryptography;
using System.Threading.Tasks;

namespace WinUIApp1.Helpers;

/// <summary>
/// AES-256-CBC 암호화/복호화 헬퍼
/// </summary>
public static class EncryptionHelper
{
    private const int KeySize = 256;
    private const int BlockSize = 128;
    private const int IvSize = 16;

    /// <summary>
    /// 파일을 AES-256-CBC로 암호화
    /// </summary>
    /// <param name="inputPath">원본 파일 경로</param>
    /// <param name="outputPath">암호화된 파일 경로</param>
    /// <param name="key">32바이트 암호화 키</param>
    public static async Task EncryptFileAsync(string inputPath, string outputPath, string key)
    {
        var keyBytes = GetKeyBytes(key);
        var iv = GenerateIv();

        using var aes = Aes.Create();
        aes.KeySize = KeySize;
        aes.BlockSize = BlockSize;
        aes.Mode = CipherMode.CBC;
        aes.Padding = PaddingMode.PKCS7;
        aes.Key = keyBytes;
        aes.IV = iv;

        await using var inputStream = File.OpenRead(inputPath);
        await using var outputStream = File.Create(outputPath);

        // IV를 파일 시작에 저장
        await outputStream.WriteAsync(iv);

        await using var cryptoStream = new CryptoStream(
            outputStream,
            aes.CreateEncryptor(),
            CryptoStreamMode.Write);

        await inputStream.CopyToAsync(cryptoStream);
    }

    /// <summary>
    /// AES-256-CBC로 암호화된 파일을 복호화
    /// </summary>
    /// <param name="inputPath">암호화된 파일 경로</param>
    /// <param name="outputPath">복호화된 파일 경로</param>
    /// <param name="key">32바이트 암호화 키</param>
    public static async Task DecryptFileAsync(string inputPath, string outputPath, string key)
    {
        var keyBytes = GetKeyBytes(key);
        var iv = new byte[IvSize];

        await using var inputStream = File.OpenRead(inputPath);

        // 파일 시작에서 IV 읽기
        await inputStream.ReadExactlyAsync(iv);

        using var aes = Aes.Create();
        aes.KeySize = KeySize;
        aes.BlockSize = BlockSize;
        aes.Mode = CipherMode.CBC;
        aes.Padding = PaddingMode.PKCS7;
        aes.Key = keyBytes;
        aes.IV = iv;

        await using var cryptoStream = new CryptoStream(
            inputStream,
            aes.CreateDecryptor(),
            CryptoStreamMode.Read);

        await using var outputStream = File.Create(outputPath);
        await cryptoStream.CopyToAsync(outputStream);
    }

    /// <summary>
    /// 스트림을 암호화하여 출력 스트림에 쓰기 (실시간 녹화용)
    /// </summary>
    public static CryptoStream CreateEncryptStream(Stream outputStream, string key, out byte[] iv)
    {
        var keyBytes = GetKeyBytes(key);
        iv = GenerateIv();

        using var aes = Aes.Create();
        aes.KeySize = KeySize;
        aes.BlockSize = BlockSize;
        aes.Mode = CipherMode.CBC;
        aes.Padding = PaddingMode.PKCS7;
        aes.Key = keyBytes;
        aes.IV = iv;

        // IV를 먼저 쓴다
        outputStream.Write(iv, 0, iv.Length);

        return new CryptoStream(outputStream, aes.CreateEncryptor(), CryptoStreamMode.Write, leaveOpen: true);
    }

    /// <summary>
    /// 키 문자열을 32바이트로 변환
    /// </summary>
    private static byte[] GetKeyBytes(string key)
    {
        var keyBytes = new byte[32];
        var sourceBytes = System.Text.Encoding.UTF8.GetBytes(key);
        Array.Copy(sourceBytes, keyBytes, Math.Min(sourceBytes.Length, keyBytes.Length));
        return keyBytes;
    }

    /// <summary>
    /// 랜덤 IV 생성
    /// </summary>
    private static byte[] GenerateIv()
    {
        var iv = new byte[IvSize];
        RandomNumberGenerator.Fill(iv);
        return iv;
    }
}
