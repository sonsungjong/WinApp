# Dot-sourced by New-PlayerMsi: native MSI dialogs need no external bootstrapper.
$uiSchema = @(
    'CREATE TABLE `Dialog` (`Dialog` CHAR(72) NOT NULL, `HCentering` SHORT NOT NULL, `VCentering` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Title` CHAR(128) LOCALIZABLE, `Control_First` CHAR(50) NOT NULL, `Control_Default` CHAR(50), `Control_Cancel` CHAR(50) PRIMARY KEY `Dialog`)',
    'CREATE TABLE `Control` (`Dialog_` CHAR(72) NOT NULL, `Control` CHAR(50) NOT NULL, `Type` CHAR(20) NOT NULL, `X` SHORT NOT NULL, `Y` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Property` CHAR(72), `Text` CHAR(0) LOCALIZABLE, `Control_Next` CHAR(50), `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Dialog_`, `Control`)',
    'CREATE TABLE `ControlEvent` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Event` CHAR(50) NOT NULL, `Argument` CHAR(255) NOT NULL, `Condition` CHAR(255), `Ordering` SHORT PRIMARY KEY `Dialog_`, `Control_`, `Event`, `Argument`, `Condition`)',
    'CREATE TABLE `ControlCondition` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Action` CHAR(50) NOT NULL, `Condition` CHAR(255) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Action`, `Condition`)',
    'CREATE TABLE `RadioButton` (`Property` CHAR(72) NOT NULL, `Order` SHORT NOT NULL, `Value` CHAR(64) NOT NULL, `X` SHORT NOT NULL, `Y` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Text` CHAR(0) LOCALIZABLE, `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Property`, `Order`)',
    'CREATE TABLE `CheckBox` (`Property` CHAR(72) NOT NULL, `Value` CHAR(64) LOCALIZABLE PRIMARY KEY `Property`)',
    'CREATE TABLE `TextStyle` (`TextStyle` CHAR(72) NOT NULL, `FaceName` CHAR(32) NOT NULL, `Size` SHORT NOT NULL, `Color` LONG, `StyleBits` SHORT PRIMARY KEY `TextStyle`)',
    'CREATE TABLE `EventMapping` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Event` CHAR(50) NOT NULL, `Attribute` CHAR(50) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Event`)',
    'CREATE TABLE `UIText` (`Key` CHAR(72) NOT NULL, `Text` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Key`)',
    'CREATE TABLE `ActionText` (`Action` CHAR(72) NOT NULL, `Description` CHAR(64) LOCALIZABLE, `Template` CHAR(128) LOCALIZABLE PRIMARY KEY `Action`)'
)
foreach ($statement in $uiSchema) {
    Invoke-MsiSql $statement
    $tableName = [regex]::Match($statement,'CREATE TABLE `([^`]+)`').Groups[1].Value
    $tableColumns[$tableName] = @([regex]::Matches($statement,'`([^`]+)` (?:CHAR\(\d+\)|SHORT|LONG|OBJECT)') | ForEach-Object { '`' + $_.Groups[1].Value + '`' })
}
Add-MsiRow 'TextStyle' @('Normal','Malgun Gothic',10,0,0)
Add-MsiRow 'TextStyle' @('Heading','Malgun Gothic',15,0,1)
Add-MsiRow 'CheckBox' @('NVP_CREATE_DESKTOP','1')
Add-MsiRow 'CheckBox' @('NVP_OPEN_DEFAULT_APPS','1')

function Add-UiControl($Dialog,$Name,$Type,$X,$Y,$Width,$Height,$Property,$Text,$Next=$null,$Attributes=3) {
    Add-MsiRow 'Control' @($Dialog,$Name,$Type,$X,$Y,$Width,$Height,$Attributes,$Property,$Text,$Next,$null)
}
function Add-UiEvent($Dialog,$Control,$Event,$Argument,$Condition='1',$Order=1) {
    Add-MsiRow 'ControlEvent' @($Dialog,$Control,$Event,$Argument,$Condition,$Order)
}
function Add-UiDialog($Name,$Heading,$Description,$First,$Default,$Cancel,$Attributes=3) {
    Add-MsiRow 'Dialog' @($Name,50,50,410,280,$Attributes,'[ProductName] 설치',$First,$Default,$Cancel)
    Add-UiControl $Name 'Heading' 'Text' 20 16 370 24 $null ('{\Heading}'+$Heading)
    Add-UiControl $Name 'Description' 'Text' 20 45 370 30 $null $Description
    Add-UiControl $Name 'BottomLine' 'Line' 0 237 410 1 $null $null
}

# Scope is selected before costing, so Windows redirects ProgramFiles64Folder,
# DesktopFolder, ProgramMenuFolder and root -1 registry entries consistently.
Add-UiDialog 'ScopeDlg' '설치 대상 선택' 'NextVideoPlayer를 사용할 사용자를 선택하세요.' 'Scope' 'Next' 'Cancel'
Add-UiControl 'ScopeDlg' 'Scope' 'RadioButtonGroup' 24 86 360 65 'NVP_SETUP_SCOPE' $null 'Next'
Add-MsiRow 'RadioButton' @('NVP_SETUP_SCOPE',1,'user',0,0,350,22,'현재 사용자만 사용',$null)
Add-MsiRow 'RadioButton' @('NVP_SETUP_SCOPE',2,'machine',0,33,350,22,'이 컴퓨터의 모든 사용자가 사용',$null)
Add-UiControl 'ScopeDlg' 'ScopeHelp' 'Text' 24 165 360 50 $null '현재 사용자: 개인 계정의 Programs 폴더에 설치합니다. 모든 사용자: Program Files에 설치하며 관리자 승인이 필요할 수 있습니다.'
Add-UiControl 'ScopeDlg' 'Next' 'PushButton' 240 249 72 20 $null '다음 >' 'Cancel'
Add-UiControl 'ScopeDlg' 'Cancel' 'PushButton' 322 249 72 20 $null '취소' 'Scope'
Add-UiEvent 'ScopeDlg' 'Next' '[ALLUSERS]' '{}' 'NVP_SETUP_SCOPE = "user"' 1
Add-UiEvent 'ScopeDlg' 'Next' '[ALLUSERS]' '1' 'NVP_SETUP_SCOPE = "machine"' 2
Add-UiEvent 'ScopeDlg' 'Next' '[MSIINSTALLPERUSER]' '1' 'NVP_SETUP_SCOPE = "user"' 3
Add-UiEvent 'ScopeDlg' 'Next' '[MSIINSTALLPERUSER]' '{}' 'NVP_SETUP_SCOPE = "machine"' 4
Add-UiEvent 'ScopeDlg' 'Next' 'EndDialog' 'Return' '1' 5
Add-UiEvent 'ScopeDlg' 'Cancel' 'EndDialog' 'Exit'

Add-UiDialog 'InstallDirDlg' '설치 위치와 바로가기' '설치 폴더, 바탕화면 바로가기 및 기본 앱 설정 여부를 확인하세요.' 'PathLabel' 'Install' 'Cancel'
Add-UiControl 'InstallDirDlg' 'PathLabel' 'Text' 24 82 360 16 $null '설치 폴더' 'Path'
Add-UiControl 'InstallDirDlg' 'Path' 'PathEdit' 24 102 360 22 'INSTALLFOLDER' $null 'Browse'
Add-UiControl 'InstallDirDlg' 'Browse' 'PushButton' 302 131 82 20 $null '찾아보기...' 'Desktop'
Add-UiControl 'InstallDirDlg' 'Desktop' 'CheckBox' 24 166 360 20 'NVP_CREATE_DESKTOP' '바탕화면에 바로가기 만들기' 'DefaultApp'
Add-UiControl 'InstallDirDlg' 'DefaultApp' 'CheckBox' 24 194 360 20 'NVP_OPEN_DEFAULT_APPS' '동영상 기본 앱으로 설정 (설치 후 Windows 기본 앱 설정 열기)' 'Install'
Add-UiControl 'InstallDirDlg' 'Install' 'PushButton' 240 249 72 20 $null '설치' 'Cancel'
Add-UiControl 'InstallDirDlg' 'Cancel' 'PushButton' 322 249 72 20 $null '취소' 'PathLabel'
Add-UiEvent 'InstallDirDlg' 'Browse' '[NVP_OLD_INSTALLFOLDER]' '[INSTALLFOLDER]'
Add-UiEvent 'InstallDirDlg' 'Browse' 'SpawnDialog' 'BrowseDlg' '1' 2
Add-UiEvent 'InstallDirDlg' 'Install' 'SetTargetPath' 'INSTALLFOLDER'
Add-UiEvent 'InstallDirDlg' 'Install' '[NVP_OPTIONS_SET]' '1' '1' 2
# An unchecked checkbox clears its property. Send an explicit zero to the
# server so the Property-table default cannot turn it back on after elevation.
Add-UiEvent 'InstallDirDlg' 'Install' '[NVP_CREATE_DESKTOP]' '0' 'NOT NVP_CREATE_DESKTOP' 3
Add-UiEvent 'InstallDirDlg' 'Install' '[NVP_OPEN_DEFAULT_APPS]' '0' 'NOT NVP_OPEN_DEFAULT_APPS' 4
Add-UiEvent 'InstallDirDlg' 'Install' 'EndDialog' 'Return' '1' 5
Add-UiEvent 'InstallDirDlg' 'Cancel' 'EndDialog' 'Exit'

Add-MsiRow 'Dialog' @('BrowseDlg',50,50,410,310,3,'설치 폴더 선택','Path','OK','Cancel')
Add-UiControl 'BrowseDlg' 'Combo' 'DirectoryCombo' 20 18 285 20 'INSTALLFOLDER' $null 'Up'
Add-UiControl 'BrowseDlg' 'Up' 'PushButton' 316 18 74 20 $null '상위 폴더' 'List'
Add-UiControl 'BrowseDlg' 'List' 'DirectoryList' 20 48 370 178 'INSTALLFOLDER' '폴더 목록' 'Path'
Add-UiControl 'BrowseDlg' 'Path' 'PathEdit' 20 238 370 22 'INSTALLFOLDER' $null 'OK'
Add-UiControl 'BrowseDlg' 'OK' 'PushButton' 232 278 74 20 $null '확인' 'Cancel'
Add-UiControl 'BrowseDlg' 'Cancel' 'PushButton' 316 278 74 20 $null '취소' 'Combo'
Add-UiEvent 'BrowseDlg' 'Up' 'DirectoryListUp' '0'
Add-UiEvent 'BrowseDlg' 'OK' 'SetTargetPath' 'INSTALLFOLDER'
Add-UiEvent 'BrowseDlg' 'OK' 'EndDialog' 'Return' '1' 2
Add-UiEvent 'BrowseDlg' 'Cancel' '[INSTALLFOLDER]' '[NVP_OLD_INSTALLFOLDER]'
Add-UiEvent 'BrowseDlg' 'Cancel' 'SetTargetPath' 'INSTALLFOLDER' '1' 2
Add-UiEvent 'BrowseDlg' 'Cancel' 'EndDialog' 'Return' '1' 3

Add-UiDialog 'MaintenanceDlg' '설치 관리' '이미 설치된 NextVideoPlayer를 복구하거나 제거할 수 있습니다.' 'Repair' 'Repair' 'Cancel'
Add-UiControl 'MaintenanceDlg' 'Repair' 'PushButton' 24 100 170 28 $null '복구' 'Remove'
Add-UiControl 'MaintenanceDlg' 'Remove' 'PushButton' 216 100 170 28 $null '제거' 'Cancel'
Add-UiControl 'MaintenanceDlg' 'Cancel' 'PushButton' 322 249 72 20 $null '취소' 'Repair'
Add-UiEvent 'MaintenanceDlg' 'Repair' '[REINSTALL]' 'ALL'
Add-UiEvent 'MaintenanceDlg' 'Repair' '[REINSTALLMODE]' 'omus' '1' 2
Add-UiEvent 'MaintenanceDlg' 'Repair' 'EndDialog' 'Return' '1' 3
Add-UiEvent 'MaintenanceDlg' 'Remove' '[REMOVE]' 'ALL'
Add-UiEvent 'MaintenanceDlg' 'Remove' 'EndDialog' 'Return' '1' 2
Add-UiEvent 'MaintenanceDlg' 'Cancel' 'EndDialog' 'Exit'

Add-UiDialog 'ProgressDlg' '설치 작업 중' '작업이 끝날 때까지 잠시 기다려 주세요.' 'Cancel' 'Cancel' 'Cancel' 1
Add-UiControl 'ProgressDlg' 'Action' 'Text' 24 104 360 30 $null '파일을 준비하고 있습니다.'
Add-UiControl 'ProgressDlg' 'Progress' 'ProgressBar' 24 145 360 18 $null $null
Add-UiControl 'ProgressDlg' 'Cancel' 'PushButton' 322 249 72 20 $null '취소' 'Cancel'
Add-UiEvent 'ProgressDlg' 'Cancel' 'SpawnDialog' 'CancelDlg'
Add-MsiRow 'EventMapping' @('ProgressDlg','Progress','SetProgress','Progress')
Add-MsiRow 'EventMapping' @('ProgressDlg','Action','ActionText','Text')
Add-MsiRow 'EventMapping' @('ProgressDlg','Cancel','Cancel','Enabled')
Add-MsiRow 'Dialog' @('CancelDlg',50,50,300,120,3,'설치 취소','No','No','No')
Add-UiControl 'CancelDlg' 'Message' 'Text' 20 20 260 34 $null '진행 중인 설치 작업을 취소하시겠습니까?'
Add-UiControl 'CancelDlg' 'Yes' 'PushButton' 120 82 72 20 $null '예' 'No'
Add-UiControl 'CancelDlg' 'No' 'PushButton' 204 82 72 20 $null '아니요' 'Yes'
Add-UiEvent 'CancelDlg' 'Yes' 'EndDialog' 'Exit'
Add-UiEvent 'CancelDlg' 'No' 'EndDialog' 'Return'

foreach ($ending in @(
    @('ExitDlg','작업 완료','요청한 설치 작업이 완료되었습니다.',-1),
    @('CancelledDlg','설치 취소','설치 작업이 취소되었습니다.',-2),
    @('FatalErrorDlg','설치 실패','오류로 인해 설치를 완료하지 못했습니다. 표시된 오류와 설치 로그를 확인해 주세요.',-3)
)) {
    Add-UiDialog $ending[0] $ending[1] $ending[2] 'Finish' 'Finish' 'Finish'
    Add-UiControl $ending[0] 'Finish' 'PushButton' 322 249 72 20 $null '닫기' 'Finish'
    if ($ending[0] -eq 'ExitDlg') {
        Add-UiEvent 'ExitDlg' 'Finish' 'DoAction' 'OpenDefaultAppsUser' ("NOT Installed AND NVP_OPEN_DEFAULT_APPS = `"1`" AND $userContext") 1
        Add-UiEvent 'ExitDlg' 'Finish' 'DoAction' 'OpenDefaultAppsMachine' ("NOT Installed AND NVP_OPEN_DEFAULT_APPS = `"1`" AND $machineContext") 2
        Add-UiEvent 'ExitDlg' 'Finish' 'EndDialog' 'Return' '1' 3
    } else {
        Add-UiEvent $ending[0] 'Finish' 'EndDialog' 'Return'
    }
    Add-MsiRow 'InstallUISequence' @($ending[0],'UILevel = 5',[int]$ending[3])
}
foreach ($action in @('InstallFiles','CreateShortcuts','WriteRegistryValues','RemoveFiles','RemoveShortcuts','RemoveRegistryValues','RegisterProduct','PublishProduct','InstallFinalize')) {
    Add-MsiRow 'ActionText' @($action,'설치 구성 요소를 적용하고 있습니다.',$null)
}
