[CmdletBinding(DefaultParameterSetName='Preview')]
param(
    [Parameter(ParameterSetName='Enable',Mandatory=$true)][switch]$Enable,
    [Parameter(ParameterSetName='Restore',Mandatory=$true)][switch]$Restore,
    [Parameter(Mandatory=$true)][string]$RuntimeRoot,
    [Parameter(Mandatory=$true)][string]$DataRoot
)
$ErrorActionPreference='Stop'
$taskRoot=$RuntimeRoot
$deviceInstance='USB\VID_045E&PID_028E\FLYDIGI_VADER4'
$bridge=Join-Path $RuntimeRoot 'FlydigiBridgeEngine.exe'
$sessionDir=Join-Path $DataRoot 'hiding'
$receiptPath=Join-Path $sessionDir 'active_session.json'
if(-not ($Enable -or $Restore)){Write-Host 'Preview. Temporary HidHide session for the exact Flydigi USB instance only; allow bridge, restore original visibility afterward. Driver installation is separate.';exit 0}
$cliCandidates=@((Join-Path ${env:ProgramFiles} 'Nefarius Software Solutions\HidHide\x64\HidHideCLI.exe'),(Join-Path ${env:ProgramFiles} 'Nefarius Software Solutions\HidHide\HidHideCLI.exe'))
$found=@($cliCandidates|Where-Object{[IO.File]::Exists($_)})
if($found.Count -ne 1){throw 'Expected one installed official HidHide CLI'}
$cli=$found[0]
$signature=Get-AuthenticodeSignature -LiteralPath $cli
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notlike '*Nefarius Software Solutions*'){throw 'Installed CLI publisher/signature mismatch'}
[IO.Directory]::CreateDirectory($sessionDir)|Out-Null
function Invoke-Cli{
    # Windows PowerShell 5 flattens ValueFromRemainingArguments to a single
    # string. Use its ordinary argument array, preserving each CLI argument.
    $cliArguments=$args
    $lines=@(& $cli @cliArguments 2>&1);if($LASTEXITCODE -ne 0){throw ('HidHide CLI failed: '+($cliArguments -join ' '))};return $lines
}
function Snapshot{
    return @{apps=@(Invoke-Cli @('--app-list'));devices=@(Invoke-Cli @('--dev-list'));cloak=@(Invoke-Cli @('--cloak-state'));inverse=@(Invoke-Cli @('--inv-state'))}
}
function Save-Session($receipt){$receipt|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $receiptPath -Encoding utf8}
function Undo-Session($receipt){
    $ownedRuntime=[IO.Path]::GetFullPath((Join-Path $DataRoot 'runtime'))+[IO.Path]::DirectorySeparatorChar
    $ownedBridge=[IO.Path]::GetFullPath($receipt.bridge)
    if($receipt.owner -ne 'FlydigiQuadBridge' -or $receipt.deviceInstance -ne $deviceInstance -or -not $ownedBridge.StartsWith($ownedRuntime,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($ownedBridge) -ne 'FlydigiBridgeEngine.exe'){throw 'Hiding receipt owner mismatch'}
    Invoke-Cli @('--dev-unhide',$deviceInstance)|Out-Null
    $now=Snapshot
    # Do not disable somebody else's newly added device hiding.
    if(@($now.devices).Count -eq 0 -and $receipt.snapshot.cloak -contains '--cloak-off'){Invoke-Cli @('--cloak-off')|Out-Null}
    if(-not $receipt.bridgePreviouslyRegistered){Invoke-Cli @('--app-unreg',$ownedBridge)|Out-Null}
    $restored=Snapshot
    if($restored.devices -contains ('--dev-hide "'+$deviceInstance+'"')){throw 'Owned dongle remains hidden'}
    $receipt.state='restored';$receipt|Add-Member -NotePropertyName restoredUtc -NotePropertyValue ([DateTime]::UtcNow.ToString('o')) -Force
    Save-Session $receipt
}
if($Restore){
    if(-not [IO.File]::Exists($receiptPath)){throw 'No owned hiding receipt'}
    $receipt=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
    if($receipt.state -eq 'restored'){Write-Host 'Owned session already restored';exit 0}
    Undo-Session $receipt;Write-Host 'Original dongle visibility restored';exit 0
}
if([IO.File]::Exists($receiptPath)){
    $old=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
    if($old.state -ne 'restored'){throw 'Previous hiding session needs restoration first'}
    Copy-Item -LiteralPath $receiptPath -Destination (Join-Path $sessionDir ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'-previous.json'))
}
$device=Get-PnpDevice -InstanceId $deviceInstance
if($device.Status -ne 'OK' -or $device.Class -ne 'XnaComposite'){throw 'Expected exact healthy Flydigi dongle'}
if((Get-FileHash -LiteralPath $bridge).Hash.ToLowerInvariant() -ne [IO.File]::ReadAllText($bridge+'.sha256').Trim()){throw 'Bridge hash mismatch'}
$before=Snapshot
if(@($before.devices).Count -ne 0 -or $before.cloak -notcontains '--cloak-off' -or $before.inverse -notcontains '--inv-off'){throw 'Existing HidHide configuration is not empty/inactive; do not alter it automatically'}
$receipt=[pscustomobject]@{owner='FlydigiQuadBridge';deviceInstance=$deviceInstance;bridge=$bridge;bridgePreviouslyRegistered=($before.apps -contains ('--app-reg "'+$bridge+'"'));snapshot=$before;utc=[DateTime]::UtcNow.ToString('o');state='prepared'}
Save-Session $receipt
try{
    Invoke-Cli @('--app-reg',$bridge)|Out-Null
    Invoke-Cli @('--dev-hide',$deviceInstance)|Out-Null
    Invoke-Cli @('--cloak-on')|Out-Null
    $after=Snapshot
    $receipt|Add-Member -NotePropertyName afterReadback -NotePropertyValue $after -Force;Save-Session $receipt
    if($after.devices.Count -ne 1 -or $after.devices[0] -ne ('--dev-hide "'+$deviceInstance+'"') -or $after.cloak -notcontains '--cloak-on'){throw ('Exact dongle hiding readback mismatch: '+($after|ConvertTo-Json -Depth 4 -Compress))}
    $receipt.state='active';Save-Session $receipt
    Write-Host 'Only the exact physical dongle is hidden; bridge is allowed. Restore when bridge stops.'
}catch{try{Undo-Session $receipt}catch{Write-Warning ('Visibility restoration needs inspection: '+$_.Exception.Message)};throw}
