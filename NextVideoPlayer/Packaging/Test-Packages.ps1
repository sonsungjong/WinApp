[CmdletBinding()]
param([string]$PackageDirectory, [switch]$SkipMsiExtraction)
. "$PSScriptRoot\Common.ps1"
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = Get-AppVersion $projectRoot
if (-not $PackageDirectory) { $PackageDirectory = Join-Path $projectRoot "artifacts\packages\$($version.ToString(3))\store" }
$info = Get-Content -LiteralPath (Join-Path $PackageDirectory 'build-info.json') -Raw | ConvertFrom-Json
$packageConfig = Get-Content -LiteralPath "$PSScriptRoot\Package.config.json" -Raw | ConvertFrom-Json
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

try {
    if ($info.PayloadDirectory -and (Test-Path -LiteralPath $info.PayloadDirectory -PathType Container)) {
        Assert-PackageFileHashes $info.PayloadDirectory
    }
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
            $fileAssoc = @($manifest.Package.Applications.Application.Extensions.Extension | Where-Object { $_.Category -eq 'windows.fileTypeAssociation' }) | Select-Object -First 1
            if (-not $fileAssoc) { throw 'MSIX manifest is missing windows.fileTypeAssociation extension.' }
            $msixTypes = @($fileAssoc.FileTypeAssociation.SupportedFileTypes.FileType)
            foreach ($ext in $packageConfig.SupportedFileTypes) {
                if ($ext -notin $msixTypes) { throw "MSIX file association is missing extension: $ext" }
            }
            Write-Host 'PASS: MSIX manifest, file associations, multiple-instance declaration and all payload SHA256 hashes.'
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
            function Read-MsiTablePairs($Database, [string]$Sql) {
                $pairs = @()
                $view = $Database.GetType().InvokeMember('OpenView',[Reflection.BindingFlags]::InvokeMethod,$null,$Database,@($Sql))
                try {
                    $null = $view.GetType().InvokeMember('Execute',[Reflection.BindingFlags]::InvokeMethod,$null,$view,@())
                    while ($rec = $view.GetType().InvokeMember('Fetch',[Reflection.BindingFlags]::InvokeMethod,$null,$view,@())) {
                        try {
                            $c1 = $rec.GetType().InvokeMember('StringData',[Reflection.BindingFlags]::GetProperty,$null,$rec,@(1))
                            $c2 = $rec.GetType().InvokeMember('StringData',[Reflection.BindingFlags]::GetProperty,$null,$rec,@(2))
                            $pairs += "$c1|$c2"
                        } finally {
                            $null = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($rec)
                        }
                    }
                } finally {
                    $null = $view.GetType().InvokeMember('Close',[Reflection.BindingFlags]::InvokeMethod,$null,$view,@())
                    $null = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view)
                }
                return $pairs
            }
            $msiInstaller = New-Object -ComObject WindowsInstaller.Installer
            $msiDb = $null
            try {
                $msiDb = $msiInstaller.GetType().InvokeMember('OpenDatabase',[Reflection.BindingFlags]::InvokeMethod,$null,$msiInstaller,@($package,0))
                $msiRegKeys = Read-MsiTablePairs $msiDb 'SELECT `Key`,`Name` FROM `Registry`'
                $msiCtrlPairs = Read-MsiTablePairs $msiDb 'SELECT `Control`,`Property` FROM `Control` WHERE `Dialog_`=''InstallDirDlg'''
            } finally {
                if ($msiDb) { $null = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($msiDb) }
                $null = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($msiInstaller)
            }
            if ('Software\RegisteredApplications|NextVideoPlayer' -notin $msiRegKeys) {
                throw 'MSI Registry table is missing RegisteredApplications entry.'
            }
            foreach ($ext in $packageConfig.SupportedFileTypes) {
                if ("Software\Classes\$ext\OpenWithProgids|NextVideoPlayer.Video" -notin $msiRegKeys -or
                    "Software\Classes\Applications\NextVideoPlayer.exe\SupportedTypes|$ext" -notin $msiRegKeys) {
                    throw "MSI Registry table is missing file association entries for $ext."
                }
            }
            if ('DefaultApp|NVP_OPEN_DEFAULT_APPS' -notin $msiCtrlPairs) {
                throw 'MSI InstallDirDlg is missing the DefaultApp checkbox.'
            }
            Write-Host 'PASS: Windows Installer administrative extraction, file associations, default-app option and all payload SHA256 hashes.'
        }
    }
    Write-Host 'Package verification passed.'
} finally {
    if (Test-Path -LiteralPath $scratch) {
        Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
    }
    $verificationRoot = Split-Path -Path $scratch -Parent
    if ($verificationRoot -and (Test-Path -LiteralPath $verificationRoot -PathType Container) -and
        -not (Get-ChildItem -LiteralPath $verificationRoot -Force | Select-Object -First 1)) {
        Remove-Item -LiteralPath $verificationRoot -Force -ErrorAction SilentlyContinue
    }
    $runsRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'artifacts\packaging\runs'))
    if ($info.PayloadDirectory) {
        $runDir = Split-Path -Path ([IO.Path]::GetFullPath($info.PayloadDirectory)) -Parent
        if ($runDir -and $runDir.StartsWith($runsRoot + '\', [StringComparison]::OrdinalIgnoreCase) -and
            (Test-Path -LiteralPath $runDir)) {
            Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    if ((Test-Path -LiteralPath $runsRoot -PathType Container) -and
        -not (Get-ChildItem -LiteralPath $runsRoot -Force | Select-Object -First 1)) {
        Remove-Item -LiteralPath $runsRoot -Force -ErrorAction SilentlyContinue
    }
}
