#requires -Version 7.0
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-PackageTool {
    param([string]$Tool, [string[]]$Arguments)
    & $Tool @Arguments | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE" }
}

function Get-PackageBuildTools {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
    }
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere.exe is required.' }
    $vs = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    if (-not $vs) { throw 'Install Visual Studio with Desktop development with C++.' }
    $vcVersion = (Get-Content -LiteralPath (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
    $redistVersion = (Get-Content -LiteralPath (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt') -Raw).Trim()
    $crt = Get-ChildItem -LiteralPath (Join-Path $vs "VC\Redist\MSVC\$redistVersion\x64") -Directory |
        Where-Object Name -Match '^Microsoft\.VC\d+\.CRT$' | Select-Object -First 1
    $sdkBase = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $sdk = Get-ChildItem -LiteralPath $sdkBase -Directory | Where-Object Name -Match '^10\.0\.\d+\.\d+$' |
        Sort-Object { [version]$_.Name } -Descending |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'x64\makeappx.exe') } | Select-Object -First 1
    if (-not $crt -or -not $sdk) { throw 'The x64 VC++ redistributable files and Windows SDK packaging tools are required.' }
    return @{
        MSBuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
        Compiler = Join-Path $vs "VC\Tools\MSVC\$vcVersion\bin\Hostx64\x64\cl.exe"
        VcVars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
        DumpBin = Join-Path $vs "VC\Tools\MSVC\$vcVersion\bin\Hostx64\x64\dumpbin.exe"
        Crt = $crt.FullName
        MakeAppx = Join-Path $sdk.FullName 'x64\makeappx.exe'
        SignTool = Join-Path $sdk.FullName 'x64\signtool.exe'
        SdkVersion = $sdk.Name
    }
}

function Get-AppVersion {
    param([string]$ProjectRoot)
    $header = Get-Content -LiteralPath (Join-Path $ProjectRoot 'Version.h') -Raw
    $parts = foreach ($part in @('MAJOR','MINOR','PATCH','REVISION')) {
        $match = [regex]::Match($header, "(?m)^#define NVP_VERSION_$part\s+(\d+)\s*$")
        if (-not $match.Success) { throw "Missing NVP_VERSION_$part in Version.h" }
        [int]$match.Groups[1].Value
    }
    if ($parts[0] -gt 255 -or $parts[1] -gt 255 -or $parts[2] -gt 65535 -or $parts[3] -ne 0) {
        throw 'Use major/minor <= 255, patch <= 65535 and revision 0 for MSI/Store version compatibility.'
    }
    return [version]($parts -join '.')
}

function Get-VerifiedArchive {
    param([string]$Url, [string]$Sha256, [string]$CacheDirectory)
    New-Item -ItemType Directory -Path $CacheDirectory -Force | Out-Null
    $archive = Join-Path $CacheDirectory ([IO.Path]::GetFileName(([uri]$Url).AbsolutePath))
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading packaging dependency: $Url"
        $temporaryPath = "$archive.$([guid]::NewGuid().ToString('N')).download"
        Invoke-WebRequest -Uri $Url -OutFile $temporaryPath
        if ((Get-FileHash -LiteralPath $temporaryPath -Algorithm SHA256).Hash -ne $Sha256) {
            throw "SHA256 mismatch: $temporaryPath"
        }
        Move-Item -LiteralPath $temporaryPath -Destination $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $Sha256) {
        throw "Cached archive failed SHA256 validation: $archive"
    }
    return $archive
}

function Assert-PayloadDependencies {
    param([string]$PayloadDirectory, [hashtable]$Tools)
    # Windows 10 system libraries only. A DLL merely existing in System32 on this
    # development PC does not make it a dependency we may omit from distribution.
    $systemNames = @('advapi32','avicap32','avrt','bcrypt','bcryptprimitives','cfgmgr32','combase','comdlg32',
        'crypt32','d2d1','d3d11','d3d12','d3dcompiler_47','dcomp','dbghelp','dnsapi','dwrite','dwmapi','dxgi',
        'gdi32','imm32','iphlpapi','kernel32','kernelbase','mf','mfplat','mfuuid','mpr','msimg32','msvcrt',
        'ncrypt','netapi32','normaliz','ntdll','ole32','oleaut32','opengl32','powrprof','propsys','psapi',
        'rpcrt4','secur32','setupapi','shcore','shell32','shlwapi','ucrtbase','user32','userenv','usp10',
        'uxtheme','version','winhttp','wininet','winmm','winspool','wintrust','wldap32','ws2_32','wtsapi32')
    $payloadNames = @(Get-ChildItem -LiteralPath $PayloadDirectory -File | ForEach-Object Name)
    foreach ($binary in Get-ChildItem -LiteralPath $PayloadDirectory -File | Where-Object Extension -In '.exe','.dll') {
        $imports = & $Tools.DumpBin /NOLOGO /DEPENDENTS $binary.FullName
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect imports: $($binary.Name)" }
        foreach ($line in $imports) {
            if ($line -match '^\s+([A-Za-z0-9_.-]+\.dll)\s*$') {
                $name = $Matches[1]
                if ($name -notin $payloadNames -and $name -notmatch '^(api|ext)-ms-' -and
                    [IO.Path]::GetFileNameWithoutExtension($name) -notin $systemNames) {
                    throw "Missing distributable dependency: $($binary.Name) -> $name"
                }
            }
        }
    }
}

function New-PackageAssets {
    param([string]$SourcePng, [string]$AssetsDirectory)
    Add-Type -AssemblyName System.Drawing
    New-Item -ItemType Directory -Path $AssetsDirectory -Force | Out-Null
    $image = [Drawing.Image]::FromFile($SourcePng)
    try {
        foreach ($asset in @(@('StoreLogo.png',50), @('Square44x44Logo.png',44), @('Square150x150Logo.png',150))) {
            $size = [int]$asset[1]
            $bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([Drawing.Color]::Transparent)
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.DrawImage($image, 0, 0, $size, $size)
                $bitmap.Save((Join-Path $AssetsDirectory $asset[0]), [Drawing.Imaging.ImageFormat]::Png)
            } finally { $graphics.Dispose(); $bitmap.Dispose() }
        }
    } finally { $image.Dispose() }
}

function Invoke-PackageSigning {
    param([string]$Path, [string]$Thumbprint, [string]$TimestampUrl, [hashtable]$Tools)
    if (-not $Thumbprint) { return }
    $arguments = @('sign','/sha1',$Thumbprint,'/fd','SHA256')
    if ($TimestampUrl) { $arguments += @('/tr',$TimestampUrl,'/td','SHA256') }
    Invoke-PackageTool $Tools.SignTool ($arguments + $Path)
    Invoke-PackageTool $Tools.SignTool @('verify','/pa',$Path)
}
