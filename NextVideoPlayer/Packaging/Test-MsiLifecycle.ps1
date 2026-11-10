#requires -Version 7.0
# Explicit integration test: installs isolated validation copies, never the actual
# product ID, and uninstalls them in finally. The player is never launched.
[CmdletBinding()]
param([string]$PackageDirectory)
. "$PSScriptRoot\Common.ps1"
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = Get-AppVersion $projectRoot
if (-not $PackageDirectory) { $PackageDirectory = Join-Path $projectRoot "artifacts\packages\$($version.ToString(3))" }
$info = Get-Content -LiteralPath (Join-Path $PackageDirectory 'build-info.json') -Raw | ConvertFrom-Json
$source = @($info.Packages | Where-Object { [IO.Path]::GetExtension($_) -eq '.msi' })
if ($source.Count -ne 1) { throw 'Build an MSI package first.' }
$testId = [guid]::NewGuid().ToString('N')
$root = [IO.Path]::GetFullPath((Join-Path $projectRoot "artifacts\packaging\lifecycle\$testId"))
$installDirectory = [IO.Path]::GetFullPath((Join-Path $root 'installed files'))
if (-not $installDirectory.StartsWith($projectRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test installation directory.' }
New-Item -ItemType Directory -Path $root -Force | Out-Null
$products = @([guid]::NewGuid().ToString('B').ToUpperInvariant(),[guid]::NewGuid().ToString('B').ToUpperInvariant())
$upgradeCode = [guid]::NewGuid().ToString('B').ToUpperInvariant()
$componentGuids = @{}
$installer = New-Object -ComObject WindowsInstaller.Installer
function Invoke-Com($Object,[string]$Name,[object[]]$Arguments=@()) {
    $raw = [object[]]::new($Arguments.Count)
    for ($i = 0; $i -lt $Arguments.Count; ++$i) { $raw[$i] = $Arguments[$i].PSObject.BaseObject }
    return $Object.GetType().InvokeMember($Name,[Reflection.BindingFlags]::InvokeMethod,$null,$Object,$raw)
}
function Get-Com($Object,[string]$Name,[object[]]$Arguments=@()) {
    $raw = [object[]]::new($Arguments.Count)
    for ($i = 0; $i -lt $Arguments.Count; ++$i) { $raw[$i] = $Arguments[$i].PSObject.BaseObject }
    return $Object.GetType().InvokeMember($Name,[Reflection.BindingFlags]::GetProperty,$null,$Object,$raw)
}
function Release-Com($Object) {
    if ($null -ne $Object) { $null = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($Object) }
}
function Execute-Sql($Database,[string]$Sql,[object[]]$Values=@()) {
    $view = Invoke-Com $Database 'OpenView' @($Sql)
    $parameterRecord = $null
    try {
        if ($Values.Count) {
            $parameterRecord = Invoke-Com $installer 'CreateRecord' @($Values.Count)
            for ($field = 0; $field -lt $Values.Count; ++$field) {
                if ($null -eq $Values[$field]) { continue }
                if ($Values[$field] -is [int]) {
                    $null = $parameterRecord.GetType().InvokeMember('IntegerData',[Reflection.BindingFlags]::SetProperty,$null,$parameterRecord,@(($field+1),[int]$Values[$field]))
                } else {
                    $null = $parameterRecord.GetType().InvokeMember('StringData',[Reflection.BindingFlags]::SetProperty,$null,$parameterRecord,@(($field+1),[string]$Values[$field]))
                }
            }
            $null = Invoke-Com $view 'Execute' @($parameterRecord)
        } else { $null = Invoke-Com $view 'Execute' }
    }
    catch { throw "Validation SQL failed: $Sql -- $($_.Exception.Message)" }
    finally { $null = Invoke-Com $view 'Close'; Release-Com $parameterRecord; Release-Com $view }
}
function Run-Msi([string[]]$Arguments,[string]$LogName) {
    $log = Join-Path $root $LogName
    $process = Start-Process -FilePath (Join-Path $env:WINDIR 'System32\msiexec.exe') -ArgumentList (
        $Arguments + @('/qn','/norestart','/l*v',('"'+$log+'"'))) -WindowStyle Hidden -Wait -PassThru
    return $process.ExitCode
}
try {
    foreach ($index in 0..1) {
        $copy = Join-Path $root "validation-$index.msi"
        Copy-Item -LiteralPath $source[0] -Destination $copy
        $database = Invoke-Com $installer 'OpenDatabase' @($copy,1)
        try {
            $productVersion = '{0}.{1}.{2}' -f $version.Major,$version.Minor,($version.Build + $index)
            foreach ($entry in (@{ProductCode=$products[$index];UpgradeCode=$upgradeCode;
                ProductVersion=$productVersion;ProductName="NextVideoPlayer Packaging Validation $testId"}).GetEnumerator()) {
                Execute-Sql $database ('UPDATE `Property` SET `Value`='''+$entry.Value+''' WHERE `Property`='''+$entry.Key+'''')
            }
            Execute-Sql $database 'DELETE FROM `Upgrade`'
            $insertUpgrade = 'INSERT INTO `Upgrade` (`UpgradeCode`,`VersionMin`,`VersionMax`,`Language`,`Attributes`,`Remove`,`ActionProperty`) VALUES (?,?,?,?,?,?,?)'
            Execute-Sql $database $insertUpgrade @($upgradeCode,$null,$productVersion,$null,1,'ALL','OLDPRODUCTS')
            Execute-Sql $database $insertUpgrade @($upgradeCode,$productVersion,$null,$null,258,$null,'NEWERPRODUCTS')
            Execute-Sql $database ('UPDATE `Directory` SET `DefaultDir`=''NVPTEST|NVP Packaging Validation '+$testId+''' WHERE `Directory`=''AppMenuFolder''')
            Execute-Sql $database ('UPDATE `Directory` SET `DefaultDir`=''NVPTEST|NVP Packaging Validation '+$testId+''' WHERE `Directory`=''INSTALLFOLDER''')
            Execute-Sql $database ('UPDATE `Registry` SET `Key`=''Software\NextVideoPlayer\PackagingValidation\'+$testId+'''')
            Execute-Sql $database ('UPDATE `RegLocator` SET `Key`=''Software\NextVideoPlayer\PackagingValidation\'+$testId+''' WHERE `Signature_`=''PreviousInstallLocation''')
            $view = Invoke-Com $database 'OpenView' @('SELECT `Component` FROM `Component`')
            $components = @()
            try {
                $null = Invoke-Com $view 'Execute'
                while ($record = Invoke-Com $view 'Fetch') {
                    try { $components += Get-Com $record 'StringData' @(1) } finally { Release-Com $record }
                }
            } finally { $null = Invoke-Com $view 'Close'; Release-Com $view }
            foreach ($component in $components) {
                if (-not $componentGuids.ContainsKey($component)) { $componentGuids[$component] = [guid]::NewGuid().ToString('B').ToUpperInvariant() }
                Execute-Sql $database ('UPDATE `Component` SET `ComponentId`='''+$componentGuids[$component]+''' WHERE `Component`='''+$component+'''')
            }
            $summary = Get-Com $database 'SummaryInformation' @(20)
            try {
                $null = $summary.GetType().InvokeMember('Property',[Reflection.BindingFlags]::SetProperty,$null,$summary,@(9,[guid]::NewGuid().ToString('B')))
                $null = Invoke-Com $summary 'Persist'
            } finally { Release-Com $summary }
            $null = Invoke-Com $database 'Commit'
        } finally { Release-Com $database }
        $exitCode = Run-Msi @('/i',('"'+$copy+'"'),('INSTALLFOLDER="'+$installDirectory+'"')) "install-$index.log"
        if ($exitCode -notin 0,3010) { throw "MSI validation installation failed ($exitCode). Logs: $root" }
        foreach ($file in $info.PayloadFiles) {
            if ((Get-FileHash -LiteralPath (Join-Path $installDirectory $file.Path)).Hash -ne $file.Sha256) { throw "Installed file mismatch: $($file.Path)" }
        }
        if ((Get-Com $installer 'ProductState' @($products[$index])) -ne 5) { throw 'Validation product is not registered.' }
        if ($index -eq 1 -and (Get-Com $installer 'ProductState' @($products[0])) -ne -1) { throw 'Upgrade did not remove previous product registration.' }
        Write-Host "PASS: isolated per-user MSI installation $productVersion, registration and installed file hashes."
    }
} finally {
    foreach ($product in $products) {
        if ((Get-Com $installer 'ProductState' @($product)) -eq 5) {
            $exitCode = Run-Msi @('/x',$product) ('uninstall-'+$product.Trim('{}')+'.log')
            if ($exitCode -notin 0,3010) { Write-Error "Validation uninstall failed ($exitCode): $product. Logs: $root" }
        }
    }
    Release-Com $installer
}
if (Test-Path -LiteralPath (Join-Path $installDirectory 'NextVideoPlayer.exe')) { throw 'Uninstall left the executable installed.' }
Write-Host "PASS: MSI upgrade and uninstall. Player was never launched. Logs: $root"
