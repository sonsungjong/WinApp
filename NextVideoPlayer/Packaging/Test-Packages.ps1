#requires -Version 7.0
[CmdletBinding()]
param([string]$PackageDirectory, [switch]$SkipMsiExtraction)
. "$PSScriptRoot\Common.ps1"
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = Get-AppVersion $projectRoot
if (-not $PackageDirectory) { $PackageDirectory = Join-Path $projectRoot "artifacts\packages\$($version.ToString(3))" }
$info = Get-Content -LiteralPath (Join-Path $PackageDirectory 'build-info.json') -Raw | ConvertFrom-Json
$scratch = Join-Path $projectRoot ('artifacts\packaging\verification\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch -Force | Out-Null
$tools = Get-PackageBuildTools

function Assert-PackageFileHashes([string]$Directory) {
    foreach ($file in $info.PayloadFiles) {
        $path = Join-Path $Directory $file.Path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.Sha256) {
            throw "Packaged file is missing or changed: $($file.Path)"
        }
        if ($file.Path -match '(?i)\.(cpp|c|h|pdb)$|(^|[\\/])config\.ini$') { throw "Development/user file in payload: $($file.Path)" }
    }
}
Assert-PackageFileHashes $info.PayloadDirectory
foreach ($package in $info.Packages) {
    if ([IO.Path]::GetExtension($package) -eq '.msix') {
        $unpacked = Join-Path $scratch 'msix'
        Invoke-PackageTool $tools.MakeAppx @('unpack','/p',$package,'/d',$unpacked,'/o')
        Assert-PackageFileHashes $unpacked
        [xml]$manifest = Get-Content -LiteralPath (Join-Path $unpacked 'AppxManifest.xml') -Raw
        if ($manifest.Package.Identity.Name -cne $info.IdentityName -or
            $manifest.Package.Identity.Publisher -cne $info.Publisher -or
            $manifest.Package.Identity.Version -ne $info.Version -or
            $manifest.Package.Identity.ProcessorArchitecture -ne 'x64') { throw 'MSIX identity/version mismatch.' }
        $expectedPublisherName = $info.PSObject.Properties['PublisherDisplayName']
        if ($info.ForStore -and -not $expectedPublisherName) { throw 'Store build is missing the publisher display name.' }
        if ($expectedPublisherName -and $manifest.Package.Properties.PublisherDisplayName -cne $expectedPublisherName.Value) {
            throw 'MSIX publisher display name mismatch.'
        }
        if ($manifest.Package.Applications.Application.GetAttribute('SupportsMultipleInstances',
            'http://schemas.microsoft.com/appx/manifest/desktop/windows10/4') -ne 'true') {
            throw 'MSIX must preserve support for multiple player instances.'
        }
        Write-Host 'PASS: MSIX manifest, multiple-instance declaration and all payload SHA256 hashes.'
    } elseif (-not $SkipMsiExtraction) {
        $extract = Join-Path $scratch 'msi'
        New-Item -ItemType Directory -Path $extract -Force | Out-Null
        $log = Join-Path $scratch 'msi-extraction.log'
        # Administrative extraction creates a source image; it does not register
        # or launch the player or install shortcuts on the development machine.
        $process = Start-Process -FilePath (Join-Path $env:WINDIR 'System32\msiexec.exe') -ArgumentList @(
            '/a',('"' + $package + '"'),'/qn','/norestart',('TARGETDIR="' + $extract + '"'),'/l*v',('"' + $log + '"')) -Wait -PassThru -WindowStyle Hidden
        if ($process.ExitCode -notin 0,3010) { throw "MSI extraction failed ($($process.ExitCode)). See $log" }
        $exe = @(Get-ChildItem -LiteralPath $extract -Filter 'NextVideoPlayer.exe' -File -Recurse)
        if ($exe.Count -ne 1) { throw 'MSI did not extract the player executable.' }
        Assert-PackageFileHashes $exe[0].Directory.FullName
        Write-Host 'PASS: Windows Installer administrative extraction and all payload SHA256 hashes.'
    }
}
Invoke-PackageTool $tools.MSBuild @((Join-Path $PSScriptRoot 'Tests\ConfigService.Tests.vcxproj'),
    '/t:Build','/p:Configuration=Release','/p:Platform=x64','/m','/v:minimal','/nologo')
$testExecutable = Join-Path $projectRoot 'artifacts\packaging\tests\bin\ConfigService.Tests.exe'
Invoke-PackageTool $testExecutable @((Join-Path $scratch 'settings with spaces'),$info.PayloadDirectory)
Write-Host "Package verification passed. Logs and extracted files: $scratch"
