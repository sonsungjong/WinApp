using System;
using System.Runtime.InteropServices;

namespace WinUIApp1.Native;

/// <summary>
/// Windows Media Foundation COM Interop 정의
/// </summary>
public static class MediaFoundationInterop
{
    // MF GUID 상수
    public static readonly Guid MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE = new("c60ac5fe-252a-478f-a0ef-bc8fa5f7cad3");
    public static readonly Guid MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID = new("8ac3587a-4ae7-42d8-99e0-0a6013eef90f");
    public static readonly Guid MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS = new("a634a91c-822b-41b9-a494-4de4643612b0");
    public static readonly Guid MF_SOURCE_READER_D3D_MANAGER = new("ec822da2-e1e9-4b29-a0d8-563c719f5269");
    public static readonly Guid MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING = new("fb394f3d-ccf1-42ee-bbb3-f9b845d5681d");
    public static readonly Guid MF_LOW_LATENCY = new("9c27891a-ed7a-40e1-88e8-b22727a024ee");

    public static readonly Guid MFMediaType_Video = new("73646976-0000-0010-8000-00AA00389B71");
    public static readonly Guid MFVideoFormat_H264 = new("34363248-0000-0010-8000-00AA00389B71");
    public static readonly Guid MFVideoFormat_NV12 = new("3231564E-0000-0010-8000-00AA00389B71");
    public static readonly Guid MFVideoFormat_RGB32 = new("00000016-0000-0010-8000-00AA00389B71");

    public static readonly Guid MF_MT_MAJOR_TYPE = new("48eba18e-f8c9-4687-bf11-0a74c9f96a8f");
    public static readonly Guid MF_MT_SUBTYPE = new("f7e34c9a-42e8-4714-b74b-cb29d72c35e5");
    public static readonly Guid MF_MT_FRAME_SIZE = new("1652c33d-d6b2-4012-b834-72030849a37d");
    public static readonly Guid MF_MT_FRAME_RATE = new("c459a2e8-3d2c-4e44-b132-fee5156c7bb0");
    public static readonly Guid MF_MT_AVG_BITRATE = new("20332624-fb0d-4d9e-bd0d-cbf6786c102e");
    public static readonly Guid MF_MT_INTERLACE_MODE = new("e2724bb8-e676-4806-b4b2-a8d6efb44ccd");

    public static readonly Guid CLSID_MFSourceResolver = new("90eab60f-e43a-4188-bcc4-e47fdf04868c");
    public static readonly Guid IID_IMFSourceResolver = new("fbe5a32d-a497-4b61-bb85-97b1a848a6e3");
    public static readonly Guid IID_IMFMediaSource = new("279a808d-aec7-40c8-9c6b-a6b492c78a66");

    // MF 함수
    [DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFStartup(uint version, uint dwFlags = 0);

    [DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFShutdown();

    [DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateAttributes(out IMFAttributes ppMFAttributes, uint cInitialSize);

    [DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateMediaType(out IMFMediaType ppMFType);

    [DllImport("mfreadwrite.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateSourceReaderFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string pwszURL,
        IMFAttributes? pAttributes,
        out IMFSourceReader ppSourceReader);

    [DllImport("mfreadwrite.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateSourceReaderFromMediaSource(
        IMFMediaSource pMediaSource,
        IMFAttributes? pAttributes,
        out IMFSourceReader ppSourceReader);

    [DllImport("mfreadwrite.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateSinkWriterFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string pwszOutputURL,
        IntPtr pByteStream,
        IMFAttributes? pAttributes,
        out IMFSinkWriter ppSinkWriter);

    [DllImport("mf.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateSourceResolver(out IMFSourceResolver ppISourceResolver);

    [DllImport("evr.dll", ExactSpelling = true, PreserveSig = false)]
    public static extern void MFCreateDXGIDeviceManager(out uint resetToken, out IMFDXGIDeviceManager ppDeviceManager);

    // MF 상수
    public const uint MF_VERSION = 0x00020070; // MF 버전 2.0
    public const uint MF_SOURCE_READER_FIRST_VIDEO_STREAM = 0xFFFFFFFC;
    public const uint MF_SOURCE_READER_FIRST_AUDIO_STREAM = 0xFFFFFFFD;
    public const uint MF_SINK_WRITER_ALL_STREAMS = 0xFFFFFFFE;

    // 헬퍼 메서드
    public static ulong PackSize(uint width, uint height) => ((ulong)width << 32) | height;
    public static ulong PackRatio(uint numerator, uint denominator) => ((ulong)numerator << 32) | denominator;
}

// COM 인터페이스 정의

[ComImport, Guid("2CD2D921-C447-44A7-A13C-4ADABFC247E3"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFAttributes
{
    void GetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr pValue);
    void GetItemType([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pType);
    void CompareItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr Value, out bool pbResult);
    void Compare(IMFAttributes pTheirs, uint MatchType, out bool pbResult);
    void GetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint punValue);
    void GetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out ulong punValue);
    void GetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out double pfValue);
    void GetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out Guid pguidValue);
    void GetStringLength([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcchLength);
    void GetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pwszValue, uint cchBufSize, out uint pcchLength);
    void GetAllocatedString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out IntPtr ppwszValue, out uint pcchLength);
    void GetBlobSize([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcbBlobSize);
    void GetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize, out uint pcbBlobSize);
    void GetAllocatedBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out IntPtr ppBuf, out uint pcbSize);
    void GetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppv);
    void SetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr Value);
    void DeleteItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey);
    void DeleteAllItems();
    void SetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, uint unValue);
    void SetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ulong unValue);
    void SetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, double fValue);
    void SetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidValue);
    void SetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPWStr)] string wszValue);
    void SetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize);
    void SetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.IUnknown)] object pUnknown);
    void LockStore();
    void UnlockStore();
    void GetCount(out uint pcItems);
    void GetItemByIndex(uint unIndex, out Guid pguidKey, IntPtr pValue);
    void CopyAllItems(IMFAttributes pDest);
}

[ComImport, Guid("44AE0FA8-EA31-4109-8D2E-4CAE4997C555"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFMediaType : IMFAttributes
{
    // IMFAttributes 메서드는 상속됨
    new void GetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr pValue);
    new void GetItemType([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pType);
    new void CompareItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr Value, out bool pbResult);
    new void Compare(IMFAttributes pTheirs, uint MatchType, out bool pbResult);
    new void GetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint punValue);
    new void GetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out ulong punValue);
    new void GetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out double pfValue);
    new void GetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out Guid pguidValue);
    new void GetStringLength([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcchLength);
    new void GetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pwszValue, uint cchBufSize, out uint pcchLength);
    new void GetAllocatedString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out IntPtr ppwszValue, out uint pcchLength);
    new void GetBlobSize([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcbBlobSize);
    new void GetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize, out uint pcbBlobSize);
    new void GetAllocatedBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out IntPtr ppBuf, out uint pcbSize);
    new void GetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppv);
    new void SetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr Value);
    new void DeleteItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey);
    new void DeleteAllItems();
    new void SetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, uint unValue);
    new void SetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ulong unValue);
    new void SetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, double fValue);
    new void SetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidValue);
    new void SetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPWStr)] string wszValue);
    new void SetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize);
    new void SetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.IUnknown)] object pUnknown);
    new void LockStore();
    new void UnlockStore();
    new void GetCount(out uint pcItems);
    new void GetItemByIndex(uint unIndex, out Guid pguidKey, IntPtr pValue);
    new void CopyAllItems(IMFAttributes pDest);

    // IMFMediaType 메서드
    void GetMajorType(out Guid pguidMajorType);
    void IsCompressedFormat(out bool pfCompressed);
    void IsEqual(IMFMediaType pIMediaType, out uint pdwFlags);
    void GetRepresentation(Guid guidRepresentation, out IntPtr ppvRepresentation);
    void FreeRepresentation(Guid guidRepresentation, IntPtr pvRepresentation);
}

[ComImport, Guid("70ae66f2-c809-4e4f-8915-bdcb406b7993"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFSourceReader
{
    void GetStreamSelection(uint dwStreamIndex, out bool pfSelected);
    void SetStreamSelection(uint dwStreamIndex, bool fSelected);
    void GetNativeMediaType(uint dwStreamIndex, uint dwMediaTypeIndex, out IMFMediaType ppMediaType);
    void GetCurrentMediaType(uint dwStreamIndex, out IMFMediaType ppMediaType);
    void SetCurrentMediaType(uint dwStreamIndex, IntPtr pdwReserved, IMFMediaType pMediaType);
    void SetCurrentPosition([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidTimeFormat, IntPtr varPosition);
    void ReadSample(uint dwStreamIndex, uint dwControlFlags, out uint pdwActualStreamIndex, out uint pdwStreamFlags, out long pllTimestamp, out IMFSample? ppSample);
    void Flush(uint dwStreamIndex);
    void GetServiceForStream(uint dwStreamIndex, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidService, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr ppvObject);
    void GetPresentationAttribute(uint dwStreamIndex, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidAttribute, IntPtr pvarAttribute);
}

[ComImport, Guid("3137f1cd-fe5e-4805-a5d8-fb477448cb3d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFSinkWriter
{
    void AddStream(IMFMediaType pTargetMediaType, out uint pdwStreamIndex);
    void SetInputMediaType(uint dwStreamIndex, IMFMediaType pInputMediaType, IMFAttributes? pEncodingParameters);
    void BeginWriting();
    void WriteSample(uint dwStreamIndex, IMFSample pSample);
    void SendStreamTick(uint dwStreamIndex, long llTimestamp);
    void PlaceMarker(uint dwStreamIndex, IntPtr pvContext);
    void NotifyEndOfSegment(uint dwStreamIndex);
    void Flush(uint dwStreamIndex);
    void Finalize_();
    void GetServiceForStream(uint dwStreamIndex, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidService, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr ppvObject);
    void GetStatistics(uint dwStreamIndex, out MF_SINK_WRITER_STATISTICS pStats);
}

[ComImport, Guid("c40a00f2-b93a-4d80-ae8c-5a1c634f58e4"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFSample : IMFAttributes
{
    // IMFAttributes 상속
    new void GetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr pValue);
    new void GetItemType([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pType);
    new void CompareItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr Value, out bool pbResult);
    new void Compare(IMFAttributes pTheirs, uint MatchType, out bool pbResult);
    new void GetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint punValue);
    new void GetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out ulong punValue);
    new void GetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out double pfValue);
    new void GetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out Guid pguidValue);
    new void GetStringLength([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcchLength);
    new void GetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pwszValue, uint cchBufSize, out uint pcchLength);
    new void GetAllocatedString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out IntPtr ppwszValue, out uint pcchLength);
    new void GetBlobSize([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcbBlobSize);
    new void GetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize, out uint pcbBlobSize);
    new void GetAllocatedBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out IntPtr ppBuf, out uint pcbSize);
    new void GetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, [MarshalAs(UnmanagedType.IUnknown)] out object ppv);
    new void SetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, IntPtr Value);
    new void DeleteItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey);
    new void DeleteAllItems();
    new void SetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, uint unValue);
    new void SetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ulong unValue);
    new void SetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, double fValue);
    new void SetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidValue);
    new void SetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPWStr)] string wszValue);
    new void SetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize);
    new void SetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.IUnknown)] object pUnknown);
    new void LockStore();
    new void UnlockStore();
    new void GetCount(out uint pcItems);
    new void GetItemByIndex(uint unIndex, out Guid pguidKey, IntPtr pValue);
    new void CopyAllItems(IMFAttributes pDest);

    // IMFSample 메서드
    void GetSampleFlags(out uint pdwSampleFlags);
    void SetSampleFlags(uint dwSampleFlags);
    void GetSampleTime(out long phnsSampleTime);
    void SetSampleTime(long hnsSampleTime);
    void GetSampleDuration(out long phnsSampleDuration);
    void SetSampleDuration(long hnsSampleDuration);
    void GetBufferCount(out uint pdwBufferCount);
    void GetBufferByIndex(uint dwIndex, out IMFMediaBuffer ppBuffer);
    void ConvertToContiguousBuffer(out IMFMediaBuffer ppBuffer);
    void AddBuffer(IMFMediaBuffer pBuffer);
    void RemoveBufferByIndex(uint dwIndex);
    void RemoveAllBuffers();
    void GetTotalLength(out uint pcbTotalLength);
    void CopyToBuffer(IMFMediaBuffer pBuffer);
}

[ComImport, Guid("045FA593-8799-42b8-BC8D-8968C6453507"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFMediaBuffer
{
    void Lock(out IntPtr ppbBuffer, out uint pcbMaxLength, out uint pcbCurrentLength);
    void Unlock();
    void GetCurrentLength(out uint pcbCurrentLength);
    void SetCurrentLength(uint cbCurrentLength);
    void GetMaxLength(out uint pcbMaxLength);
}

[ComImport, Guid("fbe5a32d-a497-4b61-bb85-97b1a848a6e3"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFSourceResolver
{
    void CreateObjectFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string pwszURL,
        uint dwFlags,
        IntPtr pProps,
        out uint pObjectType,
        [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

    void CreateObjectFromByteStream(
        IntPtr pByteStream,
        [MarshalAs(UnmanagedType.LPWStr)] string pwszURL,
        uint dwFlags,
        IntPtr pProps,
        out uint pObjectType,
        [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

    void BeginCreateObjectFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string pwszURL,
        uint dwFlags,
        IntPtr pProps,
        out IntPtr ppIUnknownCancelCookie,
        IntPtr pCallback,
        IntPtr punkState);

    void EndCreateObjectFromURL(
        IntPtr pResult,
        out uint pObjectType,
        [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

    void BeginCreateObjectFromByteStream(
        IntPtr pByteStream,
        [MarshalAs(UnmanagedType.LPWStr)] string pwszURL,
        uint dwFlags,
        IntPtr pProps,
        out IntPtr ppIUnknownCancelCookie,
        IntPtr pCallback,
        IntPtr punkState);

    void EndCreateObjectFromByteStream(
        IntPtr pResult,
        out uint pObjectType,
        [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

    void CancelObjectCreation(IntPtr pIUnknownCancelCookie);
}

[ComImport, Guid("279a808d-aec7-40c8-9c6b-a6b492c78a66"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFMediaSource
{
    void QueryInterface([In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr ppvObject);
    void AddRef();
    void Release();
    void GetEvent(uint dwFlags, out IntPtr ppEvent);
    void BeginGetEvent(IntPtr pCallback, IntPtr punkState);
    void EndGetEvent(IntPtr pResult, out IntPtr ppEvent);
    void QueueEvent(uint met, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidExtendedType, int hrStatus, IntPtr pvValue);
    void GetCharacteristics(out uint pdwCharacteristics);
    void CreatePresentationDescriptor(out IntPtr ppPresentationDescriptor);
    void Start(IntPtr pPresentationDescriptor, IntPtr pguidTimeFormat, IntPtr pvarStartPosition);
    void Stop();
    void Pause();
    void Shutdown();
}

[ComImport, Guid("eb533d5d-2db6-40f8-97a9-494692014f07"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMFDXGIDeviceManager
{
    void CloseDeviceHandle(IntPtr hDevice);
    void GetVideoService(IntPtr hDevice, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr ppService);
    void LockDevice(IntPtr hDevice, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr ppUnkDevice, bool fBlock);
    void OpenDeviceHandle(out IntPtr phDevice);
    void ResetDevice(IntPtr pUnkDevice, uint resetToken);
    void TestDevice(IntPtr hDevice);
    void UnlockDevice(IntPtr hDevice, bool fSaveState);
}

[StructLayout(LayoutKind.Sequential)]
public struct MF_SINK_WRITER_STATISTICS
{
    public uint cb;
    public long llLastTimestampReceived;
    public long llLastTimestampEncoded;
    public long llLastTimestampProcessed;
    public long llLastStreamTickReceived;
    public long llLastSinkSampleRequest;
    public ulong qwNumSamplesReceived;
    public ulong qwNumSamplesEncoded;
    public ulong qwNumSamplesProcessed;
    public ulong qwNumStreamTicksReceived;
    public uint dwByteCountQueued;
    public ulong qwByteCountProcessed;
    public uint dwNumOutstandingSinkSampleRequests;
    public uint dwAverageSampleRateReceived;
    public uint dwAverageSampleRateEncoded;
    public uint dwAverageSampleRateProcessed;
}

// MF 상수
public static class MFSourceResolverFlags
{
    public const uint MF_RESOLUTION_MEDIASOURCE = 0x00000001;
    public const uint MF_RESOLUTION_BYTESTREAM = 0x00000002;
    public const uint MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE = 0x00000010;
    public const uint MF_RESOLUTION_READ = 0x00010000;
}

public static class MFSourceReaderStreamFlags
{
    public const uint MF_SOURCE_READERF_ERROR = 0x00000001;
    public const uint MF_SOURCE_READERF_ENDOFSTREAM = 0x00000002;
    public const uint MF_SOURCE_READERF_NEWSTREAM = 0x00000004;
    public const uint MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED = 0x00000010;
    public const uint MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED = 0x00000020;
    public const uint MF_SOURCE_READERF_STREAMTICK = 0x00000100;
    public const uint MF_SOURCE_READERF_ALLEFFECTSREMOVED = 0x00000200;
}
