#requires -Version 7.0
[CmdletBinding()]
param([switch]$NoOpen)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Common.ps1"
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = Get-AppVersion $projectRoot
$packageDirectory = Join-Path $projectRoot "artifacts\packages\$($version.ToString(3))\store"

Write-Host '[1/2] 최신 소스로 MSI와 MSIX를 생성합니다.'
& "$PSScriptRoot\Build-Packages.ps1" -Target All -ForStore

Write-Host '[2/2] 설치 파일과 포함된 DLL을 검증합니다.'
& "$PSScriptRoot\Test-Packages.ps1" -PackageDirectory $packageDirectory

Write-Host "`n완료: $packageDirectory" -ForegroundColor Green
Write-Host '다른 PC에 USB로 옮겨 설치할 때는 MSI 파일을 사용하세요.'
if (-not $NoOpen) {
    Start-Process -FilePath (Join-Path $env:WINDIR 'explorer.exe') -ArgumentList ('"' + $packageDirectory + '"')
}
