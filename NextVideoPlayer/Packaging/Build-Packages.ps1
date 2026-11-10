#requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('All','MSI','MSIX')][string]$Target = 'All',
    [string]$StoreIdentityName,
    [string]$StorePublisher,
    [string]$PublisherDisplayName,
    [switch]$ForStore,
    [string]$CertificateThumbprint,
    [string]$TimestampUrl,
    [switch]$SkipBuild,
    [string]$BinaryDirectory,
    [string]$OutputDirectory
)

. "$PSScriptRoot\Common.ps1"
. "$PSScriptRoot\New-Msi.ps1"
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$config = Get-Content -LiteralPath "$PSScriptRoot\Package.config.json" -Raw | ConvertFrom-Json
$version = Get-AppVersion $projectRoot
if ($StoreIdentityName) { $config.IdentityName = $StoreIdentityName }
if ($StorePublisher) { $config.Publisher = $StorePublisher }
if ($PublisherDisplayName) { $config.PublisherDisplayName = $PublisherDisplayName }
$isDevelopmentIdentity = $config.IdentityName -eq 'NextVideoPlayer.Development' -or
    $config.Publisher -eq 'CN=NextVideoPlayer Development'
if ($ForStore -and ($Target -eq 'MSI' -or
    [string]::IsNullOrWhiteSpace($config.IdentityName) -or
    [string]::IsNullOrWhiteSpace($config.Publisher) -or
    [string]::IsNullOrWhiteSpace($config.PublisherDisplayName))) {
    throw '-ForStore requires MSIX/All and valid Partner Center identity values in Package.config.json or command-line overrides.'
}
if ($ForStore -and $isDevelopmentIdentity) {
    throw 'Development identity cannot be submitted as a Store build.'
}
if ($ForStore -and $version.Major -eq 0) {
    throw 'Microsoft Store requires a nonzero major version. Update Version.h before creating a Store package.'
}
$tools = Get-PackageBuildTools
$artifactRoot = Join-Path $projectRoot 'artifacts\packaging'
$run = Join-Path $artifactRoot ('runs\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $projectRoot "artifacts\packages\$($version.ToString(3))"
    if ($ForStore) { $OutputDirectory = Join-Path $OutputDirectory 'store' }
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $run,$OutputDirectory -Force | Out-Null
if (-not $SkipBuild) {
    $BinaryDirectory = Join-Path $run 'bin'
    $objectDirectory = Join-Path $run 'obj'
    Invoke-PackageTool $tools.MSBuild @((Join-Path $projectRoot 'NextVideoPlayer.vcxproj'),'/t:Build',
        '/p:Configuration=Release','/p:Platform=x64',"/p:OutDir=$BinaryDirectory\", "/p:IntDir=$objectDirectory\",'/m','/v:minimal','/nologo')
} elseif (-not $BinaryDirectory) { $BinaryDirectory = $projectRoot }
$exe = Join-Path $BinaryDirectory 'NextVideoPlayer.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing release executable: $exe" }
if ([Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion -ne $version.ToString()) {
    throw 'Executable version does not match Version.h. Rebuild Release x64 before using -SkipBuild.'
}

$payload = Join-Path $run 'payload'
$licenseDirectory = Join-Path $payload 'licenses'
$fontDirectory = Join-Path $payload 'ThirdParty\fonts'
New-Item -ItemType Directory -Path $payload,$licenseDirectory,$fontDirectory -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination $payload
$mpv = Join-Path $BinaryDirectory 'libmpv-2.dll'
if (-not (Test-Path -LiteralPath $mpv)) { $mpv = Join-Path $projectRoot 'ThirdParty\libmpv\libmpv-2.dll' }
Copy-Item -LiteralPath $mpv -Destination $payload
Copy-Item -LiteralPath (Join-Path $projectRoot 'NextVideoPlayer.ico') -Destination $payload
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE.txt') -Destination $licenseDirectory
Copy-Item -LiteralPath (Join-Path $projectRoot 'ThirdParty\cJSON\LICENSE') -Destination (Join-Path $licenseDirectory 'cJSON-MIT.txt')
Copy-Item -LiteralPath "$PSScriptRoot\Licenses\NanumSquare-OFL.txt" -Destination $licenseDirectory
foreach ($font in @('NanumSquareR.ttf','NanumSquareB.ttf')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "ThirdParty\fonts\$font") -Destination $fontDirectory
}
Get-ChildItem -LiteralPath $tools.Crt -Filter '*.dll' -File | Copy-Item -Destination $payload

# libmpv currently imports vulkan-1.dll even though the app renders using D3D11.
# Fetch redistributable components, never copy a graphics-driver DLL from System32.
$runtimeArchive = Get-VerifiedArchive $config.VulkanRuntime.Url $config.VulkanRuntime.Sha256 (Join-Path $artifactRoot 'cache')
$runtimeDirectory = Join-Path $run 'vulkan-runtime'
Expand-Archive -LiteralPath $runtimeArchive -DestinationPath $runtimeDirectory
$vulkan = @(Get-ChildItem -LiteralPath $runtimeDirectory -Filter 'vulkan-1.dll' -Recurse -File | Where-Object {
    $bytes = [IO.File]::ReadAllBytes($_.FullName)
    $peOffset = [BitConverter]::ToInt32($bytes,0x3c)
    [BitConverter]::ToUInt16($bytes,$peOffset + 4) -eq 0x8664
})
if ($vulkan.Count -ne 1) { throw 'Expected exactly one x64 Vulkan loader in the runtime archive.' }
Copy-Item -LiteralPath $vulkan[0].FullName -Destination $payload
$runtimeNotices = @(Get-ChildItem -LiteralPath $runtimeDirectory -Recurse -File | Where-Object Name -Match '(?i)license|copyright|notice')
if ($runtimeNotices.Count -eq 0) { throw 'Vulkan runtime license notices are missing.' }
foreach ($notice in $runtimeNotices) {
    $noticePath = Join-Path $licenseDirectory ('Vulkan\' + [IO.Path]::GetRelativePath($runtimeDirectory,$notice.FullName))
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($noticePath)) -Force | Out-Null
    Copy-Item -LiteralPath $notice.FullName -Destination $noticePath
}
$sdkScript = Get-Content -LiteralPath (Join-Path $projectRoot 'setup_sdk.bat') -Raw
$mpvSourceUrl = [regex]::Match($sdkScript,'set "ARCHIVE_URL=([^\"]+)"').Groups[1].Value
@"
NextVideoPlayer third-party component provenance

libmpv SDK archive: $mpvSourceUrl
libmpv-2.dll SHA256: $((Get-FileHash -LiteralPath (Join-Path $payload 'libmpv-2.dll')).Hash)
mpv source: https://github.com/mpv-player/mpv
Build scripts and dependency revisions: https://github.com/zhongfly/mpv-winbuild
FFmpeg source: https://ffmpeg.org/
Vulkan runtime: $($config.VulkanRuntime.Url)
Vulkan archive SHA256: $($config.VulkanRuntime.Sha256)
Vulkan loader source: https://github.com/KhronosGroup/Vulkan-Loader
cJSON source: https://github.com/DaveGamble/cJSON/tree/v1.7.19
NanumSquare: https://hangeul.naver.com/
Microsoft VC++ runtime: $([IO.Path]::GetFileName([IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($tools.Crt))))

The notices here describe bundled components, not a license for NextVideoPlayer source.
Before public distribution, provide corresponding third-party library sources and
build information as required by the licenses for the exact binaries distributed.
An upstream homepage alone does not replace those source-distribution requirements.
"@ | Set-Content -LiteralPath (Join-Path $licenseDirectory 'COMPONENT-SOURCES.txt') -Encoding utf8

if ($CertificateThumbprint) {
    foreach ($binary in Get-ChildItem -LiteralPath $payload -File | Where-Object Extension -In '.exe','.dll') {
        if ((Get-AuthenticodeSignature -LiteralPath $binary.FullName).Status -ne 'Valid') {
            Invoke-PackageSigning $binary.FullName $CertificateThumbprint $TimestampUrl $tools
        }
    }
}
Assert-PayloadDependencies $payload $tools
$filePrefix = "NextVideoPlayer-$($version.ToString(3))-x64"
$createdPackages = @()
if ($Target -in 'All','MSI') {
    $msiPath = Join-Path $run "$filePrefix.msi"
    New-PlayerMsi $payload $msiPath (Join-Path $run 'msi') $version $config
    Invoke-PackageSigning $msiPath $CertificateThumbprint $TimestampUrl $tools
    $destination = Join-Path $OutputDirectory "$filePrefix.msi"
    Copy-Item -LiteralPath $msiPath -Destination $destination -Force
    $createdPackages += $destination
}
if ($Target -in 'All','MSIX') {
    $msixDirectory = Join-Path $run 'msix'
    New-Item -ItemType Directory -Path $msixDirectory -Force | Out-Null
    Copy-Item -Path "$payload\*" -Destination $msixDirectory -Recurse
    New-PackageAssets (Join-Path $projectRoot 'img\next_video_player.png') (Join-Path $msixDirectory 'Assets')
    [xml]$manifest = Get-Content -LiteralPath "$PSScriptRoot\AppxManifest.xml" -Raw
    $manifest.Package.Identity.SetAttribute('Name',$config.IdentityName)
    $manifest.Package.Identity.SetAttribute('Publisher',$config.Publisher)
    $manifest.Package.Identity.SetAttribute('Version',$version.ToString())
    $manifest.Package.Properties.DisplayName = $config.DisplayName
    $manifest.Package.Properties.PublisherDisplayName = $config.PublisherDisplayName
    $manifest.Package.Properties.Description = $config.Description
    $manifest.Package.Dependencies.TargetDeviceFamily.SetAttribute('MinVersion',$config.MinWindowsVersion)
    $manifest.Package.Dependencies.TargetDeviceFamily.SetAttribute('MaxVersionTested',$tools.SdkVersion)
    $manifest.Package.Applications.Application.VisualElements.SetAttribute('DisplayName',$config.DisplayName)
    $manifest.Package.Applications.Application.VisualElements.SetAttribute('Description',$config.Description)
    $manifest.Save((Join-Path $msixDirectory 'AppxManifest.xml'))
    $msixPath = Join-Path $run "$filePrefix.msix"
    Invoke-PackageTool $tools.MakeAppx @('pack','/d',$msixDirectory,'/p',$msixPath,'/o')
    Invoke-PackageSigning $msixPath $CertificateThumbprint $TimestampUrl $tools
    $destination = Join-Path $OutputDirectory "$filePrefix.msix"
    Copy-Item -LiteralPath $msixPath -Destination $destination -Force
    $createdPackages += $destination
}
$hashLines = foreach ($package in $createdPackages) {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash.ToLowerInvariant(),[IO.Path]::GetFileName($package)
}
$hashLines | Set-Content -LiteralPath (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Encoding ascii
[ordered]@{Version=$version.ToString();Target=$Target;IdentityName=$config.IdentityName;Publisher=$config.Publisher;
    PublisherDisplayName=$config.PublisherDisplayName;
    ForStore=[bool]$ForStore;Signed=[bool]$CertificateThumbprint;PayloadDirectory=$payload;Packages=$createdPackages;
    PayloadFiles=@(Get-ChildItem -LiteralPath $payload -Recurse -File | ForEach-Object {
        @{Path=[IO.Path]::GetRelativePath($payload,$_.FullName);Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build-info.json') -Encoding utf8
Write-Host "Packages: $OutputDirectory"
if ($isDevelopmentIdentity -and $Target -in 'All','MSIX') { Write-Host 'MSIX uses a DEVELOPMENT identity. Configure Partner Center identity values before Store submission.' }
if (-not $CertificateThumbprint) { Write-Host 'Packages are unsigned. Store can re-sign MSIX; local MSIX installation requires a trusted signing certificate.' }
