#requires -Version 7.0
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',
      [ValidateSet('x64','Win32')][string]$Platform='x64')
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$lock=Get-Content -LiteralPath "$projectRoot\ThirdParty\libwebsockets\dependency.json" -Raw | ConvertFrom-Json
$base=Join-Path $projectRoot 'artifacts\lws'
$build=Join-Path $base "$Platform\$Configuration"
$install=Join-Path $build 'install'
$fingerprint=$lock.revision+'|'+(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
$stamp=Join-Path $install 'build-stamp.txt'
if ((Test-Path -LiteralPath "$install\lib\nvp-websockets.lib") -and
    (Test-Path -LiteralPath "$install\include\libwebsockets.h") -and
    (Test-Path -LiteralPath $stamp) -and (Get-Content -LiteralPath $stamp -Raw).Trim() -eq $fingerprint) { exit 0 }
$cmake=Get-Command cmake.exe -ErrorAction SilentlyContinue
if (-not $cmake) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=(& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
    $cmakePath=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
} else { $cmakePath=$cmake.Source }
if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'CMake is required. Install the Visual Studio C++ CMake tools.' }
New-Item -ItemType Directory -Path $base -Force | Out-Null
$archive=Join-Path $projectRoot ('artifacts\dependencies\libwebsockets-'+$lock.revision+'.zip')
if (-not (Test-Path -LiteralPath $archive)) {
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($archive)) -Force | Out-Null
    Invoke-WebRequest -Uri $lock.url -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $lock.sha256) { throw 'libwebsockets archive SHA256 mismatch.' }
$source=Join-Path ([IO.Path]::GetDirectoryName($archive)) ('libwebsockets-'+$lock.revision)
if (-not (Test-Path -LiteralPath "$source\CMakeLists.txt")) { Expand-Archive -LiteralPath $archive -DestinationPath ([IO.Path]::GetDirectoryName($archive)) }
$options=@(
    '-DLWS_WITH_STATIC=ON','-DLWS_WITH_SHARED=OFF','-DLWS_WITHOUT_TESTAPPS=ON',
    '-DLWS_WITH_MINIMAL_EXAMPLES=OFF','-DLWS_WITH_SSL=OFF','-DLWS_WITH_SCHANNEL=OFF',
    '-DLWS_WITH_HTTP2=OFF','-DLWS_WITH_HTTP3=OFF','-DLWS_ROLE_QUIC=OFF',
    '-DLWS_ROLE_WS=ON','-DLWS_ROLE_H1=ON','-DLWS_ROLE_MQTT=OFF',
    '-DLWS_WITHOUT_CLIENT=ON','-DLWS_WITHOUT_EXTENSIONS=ON','-DLWS_WITH_ZLIB=OFF',
    '-DLWS_WITH_HTTP_STREAM_COMPRESSION=OFF','-DLWS_WITH_SYS_ASYNC_DNS=OFF',
    '-DLWS_WITH_SYS_INFO=OFF','-DLWS_WITH_SYS_SMD=OFF','-DLWS_WITH_SYS_STATE=OFF',
    '-DLWS_WITH_UPNG=OFF','-DLWS_WITH_JPEG=OFF','-DLWS_WITH_SVG=OFF','-DLWS_WITH_GIF=OFF',
    '-DLWS_WITH_DLO=OFF','-DLWS_WITH_GZINFLATE=OFF','-DLWS_WITH_SECURE_STREAMS=OFF',
    '-DLWS_WITH_LEJP=OFF','-DLWS_WITH_LECP=OFF','-DLWS_WITH_GENCRYPTO=OFF',
    '-DLWS_WITH_THREADPOOL=OFF','-DLWS_WITH_ASYNC_QUEUE=OFF','-DLWS_MAX_SMP=1',
    '-DLWS_WITH_PLUGINS=OFF','-DLWS_WITH_PLUGINS_BUILTIN=OFF','-DLWS_WITH_LWSWS=OFF',
    '-DLWS_WITH_CGI=OFF','-DLWS_WITH_HTTP_PROXY=OFF','-DLWS_WITH_HTTP_BASIC_AUTH=OFF',
    '-DLWS_WITH_HTTP_DIGEST_AUTH=OFF','-DDISABLE_WERROR=ON','-DCMAKE_DEBUG_POSTFIX=',
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'
)
& $cmakePath -S $source -B $build -G 'Visual Studio 18 2026' -A $Platform -T v145 "-DCMAKE_INSTALL_PREFIX=$install" @options
if ($LASTEXITCODE) { throw 'libwebsockets configure failed.' }
& $cmakePath --build $build --config $Configuration --target install --parallel 4
if ($LASTEXITCODE) { throw 'libwebsockets build failed.' }
$library=Get-ChildItem -LiteralPath "$install\lib" -Filter '*websockets*.lib' -File | Select-Object -First 1
if (-not $library) { throw 'libwebsockets static library was not generated.' }
Copy-Item -LiteralPath $library.FullName -Destination "$install\lib\nvp-websockets.lib" -Force
[IO.File]::WriteAllText($stamp,$fingerprint,[Text.UTF8Encoding]::new($false))
