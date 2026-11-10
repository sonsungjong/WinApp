#requires -Version 7.0
# Author a standard Windows Installer database through the Windows SDK automation
# API. No WiX extension, global tool installation, executable custom action, or
# third-party build-tool subscription is required.
function New-PlayerMsi {
    param([string]$PayloadDirectory, [string]$OutputPath, [string]$WorkDirectory,
          [version]$Version, [pscustomobject]$Config)

    function Invoke-MsiMethod($Object, [string]$Name, [object[]]$Arguments = @()) {
        $raw = [object[]]::new($Arguments.Count)
        for ($i = 0; $i -lt $Arguments.Count; ++$i) { $raw[$i] = $Arguments[$i].PSObject.BaseObject }
        return $Object.GetType().InvokeMember($Name, [Reflection.BindingFlags]::InvokeMethod, $null, $Object, $raw)
    }
    function Set-MsiProperty($Object, [string]$Name, [object[]]$Arguments) {
        $raw = [object[]]::new($Arguments.Count)
        for ($i = 0; $i -lt $Arguments.Count; ++$i) { $raw[$i] = $Arguments[$i].PSObject.BaseObject }
        $null = $Object.GetType().InvokeMember($Name, [Reflection.BindingFlags]::SetProperty, $null, $Object, $raw)
    }
    function Release-MsiObject($Object) {
        if ($null -ne $Object) { $null = [Runtime.InteropServices.Marshal]::FinalReleaseComObject($Object) }
    }
    function Get-StableMsiGuid([string]$Name) {
        $bytes = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes("NextVideoPlayer:$Name"))
        return ([guid]::new([byte[]]$bytes[0..15])).ToString('B').ToUpperInvariant()
    }
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $database = $null
    $tableColumns = @{}
    function Invoke-MsiSql([string]$Sql, [object[]]$Values = @()) {
        $view = $null; $record = $null
        try {
            $view = Invoke-MsiMethod $database 'OpenView' @($Sql)
            if ($Values.Count) {
                $record = Invoke-MsiMethod $installer 'CreateRecord' @($Values.Count)
                for ($i = 0; $i -lt $Values.Count; ++$i) {
                    $value = $Values[$i]
                    if ($null -eq $value -or ($value -is [string] -and $value.Length -eq 0)) { continue }
                    if ($value -is [int]) { Set-MsiProperty $record 'IntegerData' @(($i + 1), $value) }
                    else { Set-MsiProperty $record 'StringData' @(($i + 1), [string]$value) }
                }
                $null = Invoke-MsiMethod $view 'Execute' @($record)
            } else { $null = Invoke-MsiMethod $view 'Execute' }
        } catch { throw "MSI SQL failed: $Sql -- $($_.Exception.Message)" }
        finally {
            if ($view) { $null = Invoke-MsiMethod $view 'Close' }
            Release-MsiObject $record; Release-MsiObject $view
        }
    }
    function Add-MsiRow([string]$Table, [object[]]$Values) {
        $placeholders = (@('?') * $Values.Count) -join ','
        $columns = $tableColumns[$Table]
        if ($columns.Count -ne $Values.Count) { throw "Incorrect column count for $Table" }
        Invoke-MsiSql ('INSERT INTO `' + $Table + '` (' + ($columns -join ',') + ') VALUES (' + $placeholders + ')') $Values
    }
    function Add-MsiStream([string]$Table, [string]$Key, [string]$Path) {
        $view = $null; $record = $null
        try {
            $view = Invoke-MsiMethod $database 'OpenView' @('INSERT INTO `' + $Table + '` (`Name`,`Data`) VALUES (?,?)')
            $record = Invoke-MsiMethod $installer 'CreateRecord' @(2)
            Set-MsiProperty $record 'StringData' @(1,$Key)
            $null = Invoke-MsiMethod $record 'SetStream' @(2,$Path)
            $null = Invoke-MsiMethod $view 'Execute' @($record)
        } finally {
            if ($view) { $null = Invoke-MsiMethod $view 'Close' }
            Release-MsiObject $record; Release-MsiObject $view
        }
    }
    $files = @(Get-ChildItem -LiteralPath $PayloadDirectory -Recurse -File | Sort-Object FullName)
    New-Item -ItemType Directory -Path $WorkDirectory -Force | Out-Null
    $cabDirectory = Join-Path $WorkDirectory 'cab'
    New-Item -ItemType Directory -Path $cabDirectory -Force | Out-Null
    $ddf = @('.OPTION EXPLICIT', '.Set Cabinet=ON', '.Set Compress=ON', '.Set CompressionType=LZX',
        '.Set CompressionMemory=21', '.Set MaxDiskSize=0', '.Set CabinetNameTemplate=payload.cab',
        ('.Set DiskDirectoryTemplate="' + [IO.Path]::GetRelativePath($PayloadDirectory,$cabDirectory) + '"'),
        ('.Set RptFileName="' + [IO.Path]::GetRelativePath($PayloadDirectory,"$WorkDirectory\makecab.rpt") + '"'),
        ('.Set InfFileName="' + [IO.Path]::GetRelativePath($PayloadDirectory,"$WorkDirectory\makecab.inf") + '"'))
    for ($i = 0; $i -lt $files.Count; ++$i) {
        $ddf += ('"{0}" F{1:D4}' -f [IO.Path]::GetRelativePath($PayloadDirectory,$files[$i].FullName), ($i + 1))
    }
    $ddfPath = Join-Path $WorkDirectory 'payload.ddf'
    $ddf | Set-Content -LiteralPath $ddfPath -Encoding ascii
    Push-Location $PayloadDirectory
    try {
        $cabLog = Join-Path $WorkDirectory 'makecab-console.log'
        & (Join-Path $env:WINDIR 'System32\makecab.exe') /F $ddfPath /V0 *> $cabLog
        if ($LASTEXITCODE -ne 0) { throw "Cabinet generation failed. See $cabLog" }
    }
    finally { Pop-Location }

    try {
        $database = Invoke-MsiMethod $installer 'OpenDatabase' @($OutputPath,3)
        $schema = @(
            'CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)',
            'CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)',
            'CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)',
            'CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)',
            'CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)',
            'CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` LONG NOT NULL PRIMARY KEY `File`)',
            'CREATE TABLE `MsiFileHash` (`File_` CHAR(72) NOT NULL, `Options` SHORT NOT NULL, `HashPart1` LONG NOT NULL, `HashPart2` LONG NOT NULL, `HashPart3` LONG NOT NULL, `HashPart4` LONG NOT NULL PRIMARY KEY `File_`)',
            'CREATE TABLE `CreateFolder` (`Directory_` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Directory_`, `Component_`)',
            'CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` LONG NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)',
            'CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)',
            'CREATE TABLE `Shortcut` (`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL LOCALIZABLE, `Component_` CHAR(72) NOT NULL, `Target` CHAR(255) NOT NULL, `Arguments` CHAR(255), `Description` CHAR(255) LOCALIZABLE, `Hotkey` SHORT, `Icon_` CHAR(72), `IconIndex` SHORT, `ShowCmd` SHORT, `WkDir` CHAR(72), `DisplayResourceDLL` CHAR(255), `DisplayResourceId` SHORT, `DescriptionResourceDLL` CHAR(255), `DescriptionResourceId` SHORT PRIMARY KEY `Shortcut`)',
            'CREATE TABLE `RemoveFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) LOCALIZABLE, `DirProperty` CHAR(72) NOT NULL, `InstallMode` SHORT NOT NULL PRIMARY KEY `FileKey`)',
            'CREATE TABLE `Icon` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)',
            'CREATE TABLE `Upgrade` (`UpgradeCode` CHAR(38) NOT NULL, `VersionMin` CHAR(20), `VersionMax` CHAR(20), `Language` CHAR(255), `Attributes` LONG NOT NULL, `Remove` CHAR(255), `ActionProperty` CHAR(72) NOT NULL PRIMARY KEY `UpgradeCode`, `VersionMin`, `VersionMax`, `Language`, `Attributes`)',
            'CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`)',
            'CREATE TABLE `AppSearch` (`Property` CHAR(72) NOT NULL, `Signature_` CHAR(72) NOT NULL PRIMARY KEY `Property`, `Signature_`)',
            'CREATE TABLE `RegLocator` (`Signature_` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Type` SHORT PRIMARY KEY `Signature_`)',
            'CREATE TABLE `Signature` (`Signature` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `MinVersion` CHAR(20), `MaxVersion` CHAR(20), `MinSize` LONG, `MaxSize` LONG, `MinDate` LONG, `MaxDate` LONG, `Languages` CHAR(255) PRIMARY KEY `Signature`)',
            'CREATE TABLE `CompLocator` (`Signature_` CHAR(72) NOT NULL, `ComponentId` CHAR(38) NOT NULL, `Type` SHORT PRIMARY KEY `Signature_`)',
            'CREATE TABLE `IniLocator` (`Signature_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `Section` CHAR(96) NOT NULL, `Key` CHAR(128) NOT NULL, `Field` SHORT, `Type` SHORT PRIMARY KEY `Signature_`)',
            'CREATE TABLE `DrLocator` (`Signature_` CHAR(72) NOT NULL, `Parent` CHAR(72), `Path` CHAR(255), `Depth` SHORT PRIMARY KEY `Signature_`, `Parent`, `Path`)',
            'CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(0), `ExtendedType` LONG PRIMARY KEY `Action`)',
            'CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)',
            'CREATE TABLE `InstallUISequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)',
            'CREATE TABLE `AdminExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)',
            'CREATE TABLE `AdminUISequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)'
        )
        foreach ($statement in $schema) {
            Invoke-MsiSql $statement
            $tableName = [regex]::Match($statement,'CREATE TABLE `([^`]+)`').Groups[1].Value
            $tableColumns[$tableName] = @([regex]::Matches($statement,'`([^`]+)` (?:CHAR\(\d+\)|SHORT|LONG|OBJECT)') | ForEach-Object { '`' + $_.Groups[1].Value + '`' })
        }
        $productVersion = '{0}.{1}.{2}' -f $Version.Major,$Version.Minor,$Version.Build
        $properties = [ordered]@{
            ProductCode = Get-StableMsiGuid "Product:$productVersion"
            UpgradeCode = $Config.MsiUpgradeCode
            ProductName = $Config.DisplayName
            ProductVersion = $productVersion
            ProductLanguage = '1033'
            Manufacturer = $Config.Manufacturer
            INSTALLLEVEL = '1'
            ARPNOMODIFY = '1'
            ARPPRODUCTICON = 'AppIcon'
            REBOOT = 'ReallySuppress'
            SecureCustomProperties = 'OLDPRODUCTS;NEWERPRODUCTS;INSTALLFOLDER;NVP_PREVIOUS_INSTALLFOLDER;NVP_WINDOWS_BUILD'
        }
        foreach ($entry in $properties.GetEnumerator()) { Add-MsiRow 'Property' @($entry.Key,$entry.Value) }
        Add-MsiRow 'Directory' @('TARGETDIR',$null,'SourceDir')
        Add-MsiRow 'Directory' @('LocalAppDataFolder','TARGETDIR','.')
        Add-MsiRow 'Directory' @('AppProgramsFolder','LocalAppDataFolder','Programs')
        Add-MsiRow 'Directory' @('INSTALLFOLDER','AppProgramsFolder','NextVideoPlayer')
        Add-MsiRow 'Directory' @('ProgramMenuFolder','TARGETDIR','.')
        Add-MsiRow 'Directory' @('AppMenuFolder','ProgramMenuFolder','NextVideoPlayer')
        Add-MsiRow 'Feature' @('MainFeature',$null,$Config.DisplayName,$Config.Description,1,1,'INSTALLFOLDER',0)
        $directories = @{ '' = 'INSTALLFOLDER' }
        for ($i = 0; $i -lt $files.Count; ++$i) {
            $file = $files[$i]
            $relative = [IO.Path]::GetRelativePath($PayloadDirectory,$file.FullName)
            $parent = [IO.Path]::GetDirectoryName($relative)
            $parts = if ($parent) { @($parent.Split('\')) } else { @() }
            $current = ''; $parentId = 'INSTALLFOLDER'
            foreach ($part in $parts) {
                $current = if ($current) { "$current\$part" } else { $part }
                if (-not $directories.ContainsKey($current)) {
                    $directoryId = 'D{0:D4}' -f $directories.Count
                    Add-MsiRow 'Directory' @($directoryId,$parentId,("$directoryId|$part"))
                    $directories[$current] = $directoryId
                }
                $parentId = $directories[$current]
            }
            $fileId = 'F{0:D4}' -f ($i + 1); $componentId = 'C{0:D4}' -f ($i + 1)
            $registryId = 'R{0:D4}' -f ($i + 1)
            # Per-user components need an HKCU key path, including file components.
            Add-MsiRow 'Component' @($componentId,(Get-StableMsiGuid "Component:$relative"),$parentId,260,$null,$registryId)
            Add-MsiRow 'Registry' @($registryId,1,'Software\NextVideoPlayer\Installer\Components',$relative,'#1',$componentId)
            Add-MsiRow 'FeatureComponents' @('MainFeature',$componentId)
            $fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($file.FullName).FileVersion
            if ($fileVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') { $fileVersion = $null }
            Add-MsiRow 'File' @($fileId,$componentId,("$fileId|$($file.Name)"),[int]$file.Length,$fileVersion,$null,0,($i + 1))
            if (-not $fileVersion) {
                $hashRecord = Invoke-MsiMethod $installer 'FileHash' @($file.FullName,0)
                try {
                    $hashValues = @($fileId,0)
                    foreach ($part in 1..4) {
                        $hashValues += $hashRecord.GetType().InvokeMember('IntegerData',[Reflection.BindingFlags]::GetProperty,$null,$hashRecord,@($part))
                    }
                    Add-MsiRow 'MsiFileHash' $hashValues
                } finally { Release-MsiObject $hashRecord }
            }
        }
        foreach ($directoryId in $directories.Values) { Add-MsiRow 'CreateFolder' @($directoryId,'C0001') }
        Add-MsiRow 'Component' @('StartMenu',(Get-StableMsiGuid 'Component:StartMenu'),'AppMenuFolder',260,$null,'InstalledMarker')
        Add-MsiRow 'FeatureComponents' @('MainFeature','StartMenu')
        Add-MsiRow 'Registry' @('InstalledMarker',1,'Software\NextVideoPlayer\Installer','Installed','#1','StartMenu')
        # Remember the selected directory for repair, upgrade and uninstall.
        # Registry key paths do not let MSI infer an overridden INSTALLFOLDER.
        Add-MsiRow 'Registry' @('InstallDirectory',1,'Software\NextVideoPlayer\Installer','InstallDirectory','[INSTALLFOLDER]','StartMenu')
        Add-MsiRow 'AppSearch' @('NVP_PREVIOUS_INSTALLFOLDER','PreviousInstallLocation')
        Add-MsiRow 'RegLocator' @('PreviousInstallLocation',1,'Software\NextVideoPlayer\Installer','InstallDirectory',18)
        Add-MsiRow 'AppSearch' @('NVP_WINDOWS_BUILD','WindowsBuild')
        Add-MsiRow 'RegLocator' @('WindowsBuild',2,'SOFTWARE\Microsoft\Windows NT\CurrentVersion','CurrentBuildNumber',18)
        Add-MsiRow 'CustomAction' @('RestoreInstallDirectory',51,'INSTALLFOLDER','[NVP_PREVIOUS_INSTALLFOLDER]',$null)
        Add-MsiRow 'CustomAction' @('SetArpInstallLocation',51,'ARPINSTALLLOCATION','[INSTALLFOLDER]',$null)
        Add-MsiRow 'Shortcut' @('AppShortcut','AppMenuFolder','NVP|NextVideoPlayer','StartMenu',
            '[INSTALLFOLDER]NextVideoPlayer.exe',$null,$Config.Description,$null,'AppIcon',0,1,'INSTALLFOLDER',$null,$null,$null,$null)
        Add-MsiRow 'RemoveFile' @('RemoveAppMenu','StartMenu',$null,'AppMenuFolder',2)
        Add-MsiRow 'Media' @(1,$files.Count,$null,'#payload.cab',$null,$null)
        Add-MsiStream 'Icon' 'AppIcon' (Join-Path $PayloadDirectory 'NextVideoPlayer.ico')
        Add-MsiStream '_Streams' 'payload.cab' (Join-Path $cabDirectory 'payload.cab')
        Add-MsiRow 'Upgrade' @($Config.MsiUpgradeCode,$null,$productVersion,$null,1,'ALL','OLDPRODUCTS')
        Add-MsiRow 'Upgrade' @($Config.MsiUpgradeCode,$productVersion,$null,$null,258,$null,'NEWERPRODUCTS')
        $minBuild = ([version]$Config.MinWindowsVersion).Build
        Add-MsiRow 'LaunchCondition' @("Installed OR (VersionNT64 AND NVP_WINDOWS_BUILD >= $minBuild)",'NextVideoPlayer requires 64-bit Windows 10 2004 or later.')
        Add-MsiRow 'LaunchCondition' @('Installed OR NOT NEWERPRODUCTS','A newer version of NextVideoPlayer is already installed.')
        $executeActions = [ordered]@{FindRelatedProducts=25;AppSearch=50;RestoreInstallDirectory=70;LaunchConditions=100;ValidateProductID=700;
            CostInitialize=800;FileCost=900;CostFinalize=1000;SetArpInstallLocation=1100;MigrateFeatureStates=1200;InstallValidate=1400;
            InstallInitialize=1500;RemoveExistingProducts=1510;ProcessComponents=1600;UnpublishFeatures=1800;
            RemoveRegistryValues=2600;RemoveShortcuts=3200;RemoveFiles=3500;RemoveFolders=3600;
            CreateFolders=3700;InstallFiles=4000;CreateShortcuts=4500;WriteRegistryValues=5000;
            RegisterUser=6000;RegisterProduct=6100;PublishFeatures=6300;PublishProduct=6400;InstallFinalize=6600}
        foreach ($action in $executeActions.GetEnumerator()) {
            $condition = switch ($action.Key) {
                'RemoveExistingProducts' { 'OLDPRODUCTS' }
                'RestoreInstallDirectory' { 'NOT INSTALLFOLDER AND NVP_PREVIOUS_INSTALLFOLDER' }
                default { $null }
            }
            Add-MsiRow 'InstallExecuteSequence' @($action.Key,$condition,$action.Value)
        }
        foreach ($action in ([ordered]@{FindRelatedProducts=25;AppSearch=50;RestoreInstallDirectory=70;LaunchConditions=100;ValidateProductID=700;
            CostInitialize=800;FileCost=900;CostFinalize=1000;ExecuteAction=1300}).GetEnumerator()) {
            $condition = if ($action.Key -eq 'RestoreInstallDirectory') { 'NOT INSTALLFOLDER AND NVP_PREVIOUS_INSTALLFOLDER' } else { $null }
            Add-MsiRow 'InstallUISequence' @($action.Key,$condition,$action.Value)
        }
        foreach ($action in ([ordered]@{CostInitialize=800;FileCost=900;CostFinalize=1000;
            InstallValidate=1400;InstallInitialize=1500;InstallAdminPackage=3900;InstallFiles=4000;InstallFinalize=6600}).GetEnumerator()) {
            Add-MsiRow 'AdminExecuteSequence' @($action.Key,$null,$action.Value)
        }
        foreach ($action in ([ordered]@{CostInitialize=800;FileCost=900;CostFinalize=1000;ExecuteAction=1300}).GetEnumerator()) {
            Add-MsiRow 'AdminUISequence' @($action.Key,$null,$action.Value)
        }
        $summary = $database.GetType().InvokeMember('SummaryInformation',[Reflection.BindingFlags]::GetProperty,$null,$database,@(20))
        try {
            foreach ($entry in (@{2='Installation Database';3=$Config.DisplayName;4=$Config.Manufacturer;
                7='x64;1033';9=([guid]::NewGuid().ToString('B').ToUpperInvariant());14=500;15=10;18='NextVideoPlayer Packaging'}).GetEnumerator()) {
                Set-MsiProperty $summary 'Property' @([int]$entry.Key,$entry.Value)
            }
            $null = Invoke-MsiMethod $summary 'Persist'
        } finally { Release-MsiObject $summary }
        $null = Invoke-MsiMethod $database 'Commit'
        Write-Host "Created MSI: $OutputPath"
    } finally { Release-MsiObject $database; Release-MsiObject $installer }
}
