#requires -Version 7.0
[CmdletBinding()]
param(
    [uri]$Endpoint = 'ws://127.0.0.1:49152/api/v1',
    [string]$Message = '{"type":"all"}'
)

$ErrorActionPreference = 'Stop'
$command = ConvertFrom-Json -InputObject $Message -AsHashtable
if ($command -isnot [Collections.IDictionary]) { throw 'Message must be a JSON object.' }
if (-not $command.ContainsKey('requestId') -or [string]::IsNullOrEmpty($command.requestId)) {
    $command.requestId = [guid]::NewGuid().ToString('N')
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
        $result = ConvertFrom-Json -InputObject $text -AsHashtable
        if ($result.requestId -eq $command.requestId) {
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
