[CmdletBinding()]
param(
    [uri]$Endpoint = 'ws://127.0.0.1:49152/api/v1',
    [string]$Message = '{"type":"all"}'
)

$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Core') {
    $cleanedModulePaths = @(foreach ($entry in ($env:PSModulePath -split ';')) {
        if ($entry -and $entry -notmatch '(?i)\\PowerShell\\') { $entry }
    })
    $sysModules = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules'
    if ($cleanedModulePaths -notcontains $sysModules) { $cleanedModulePaths += $sysModules }
    $env:PSModulePath = $cleanedModulePaths -join ';'
}
$command = ConvertFrom-Json -InputObject $Message
if ($null -eq $command -or $command -isnot [Management.Automation.PSCustomObject]) { throw 'Message must be a JSON object.' }
$requestIdProp = $command.PSObject.Properties['requestId']
if (-not $requestIdProp -or [string]::IsNullOrEmpty([string]$requestIdProp.Value)) {
    $requestId = [guid]::NewGuid().ToString('N')
    $command | Add-Member -MemberType NoteProperty -Name 'requestId' -Value $requestId -Force
} else {
    $requestId = [string]$requestIdProp.Value
}
$body = [Text.Encoding]::UTF8.GetBytes(($command | ConvertTo-Json -Compress -Depth 32))
$client = [Net.WebSockets.ClientWebSocket]::new()
$timeout = [Threading.CancellationTokenSource]::new(5000)
$response = [IO.MemoryStream]::new()
try {
    $null = $client.ConnectAsync($Endpoint, $timeout.Token).GetAwaiter().GetResult()
    $null = $client.SendAsync([ArraySegment[byte]]::new($body), [Net.WebSockets.WebSocketMessageType]::Text,
        $true, $timeout.Token).GetAwaiter().GetResult()
    $buffer = [byte[]]::new(8192)
    while ($true) {
        $part = $client.ReceiveAsync([ArraySegment[byte]]::new($buffer), $timeout.Token).GetAwaiter().GetResult()
        if ($part.MessageType -eq [Net.WebSockets.WebSocketMessageType]::Close) {
            throw 'The player closed the API connection.'
        }
        $response.Write($buffer, 0, $part.Count)
        if ($response.Length -gt 1MB) { throw 'Response exceeds 1 MiB.' }
        if (-not $part.EndOfMessage) { continue }
        $text = [Text.Encoding]::UTF8.GetString($response.ToArray())
        $result = ConvertFrom-Json -InputObject $text
        $resultIdProp = if ($null -ne $result) { $result.PSObject.Properties['requestId'] } else { $null }
        if ($resultIdProp -and [string]$resultIdProp.Value -eq $requestId) {
            try {
                if ([Console]::IsOutputRedirected) { [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false) }
            } catch {}
            Write-Output $text
            break
        }
        $response.SetLength(0)
    }
} finally {
    $response.Dispose()
    $timeout.Dispose()
    $client.Dispose()
}
