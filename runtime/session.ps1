[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Bridge','Test','Requirements','Recover','VirtualCheck','SetupWinUHid','SetupHidHide','SetupGameInput','RemoveWinUHid','ValidateSetup','SetupExistingCheck')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$RuntimeRoot,
    [Parameter(Mandatory=$true)][string]$DataRoot,
    [Parameter(Mandatory=$true)][string]$SessionDir,
    [Parameter(Mandatory=$true)][string]$StopEvent,
    [Parameter(Mandatory=$true)][int]$ParentPid,
    [ValidateRange(0,86400)][int]$DurationSeconds=0,
    [switch]$DiagnosticLogging
)
$ErrorActionPreference='Stop'
$engine=Join-Path $RuntimeRoot 'FlydigiBridgeEngine.exe'
$lifecycle=Join-Path $SessionDir 'session.jsonl'
$nativeLog=Join-Path $SessionDir 'engine.jsonl'
$recoveryLog=Join-Path $SessionDir 'original-profile.jsonl'
$pendingPath=Join-Path $DataRoot 'pending-restore.json'
$serviceName='Flydigi Space Station Service'
$nativeCode=1;$serviceTouched=$false;$restoreRunning=$false;$hidingTouched=$false;$pending=$null;$errorText=$null
$physical=$Mode -in @('Bridge','Test','Recover')
$sessionMutex=$null;$mutexHeld=$false
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Record($event,$detail){@{event=$event;utc=[DateTime]::UtcNow.ToString('o');detail=$detail}|ConvertTo-Json -Depth 7 -Compress|Add-Content -LiteralPath $lifecycle -Encoding utf8}
function Native($arguments){
    $savedAction=$ErrorActionPreference
    try{$ErrorActionPreference='Continue'; & $engine @arguments 2>&1 | ForEach-Object{[string]$_ | Add-Content -LiteralPath (Join-Path $SessionDir 'console.log') -Encoding utf8}; return $LASTEXITCODE}
    finally{$ErrorActionPreference=$savedAction}
}
function OwnedPath($path){
    $root=[IO.Path]::GetFullPath((Join-Path $DataRoot 'sessions'))+[IO.Path]::DirectorySeparatorChar
    return [IO.Path]::GetFullPath($path).StartsWith($root,[StringComparison]::OrdinalIgnoreCase)
}
function PendingBackup($receipt){
    if($receipt.owner -ne 'FlydigiBridge'){throw 'Recovery receipt does not belong to this application'}
    $candidate=$receipt.originalLog
    if(-not [IO.File]::Exists($candidate) -and $receipt.fallbackLog){$candidate=$receipt.fallbackLog}
    if(-not (OwnedPath $candidate) -or [IO.Path]::GetFileName($candidate) -ne 'original-profile.jsonl'){throw 'Recovery backup path is outside the owned session folder'}
    if(-not [IO.File]::Exists($candidate)){return $null}
    if($receipt.sha256 -and $candidate -eq $receipt.originalLog -and (Hash $candidate) -ne $receipt.sha256){throw 'Original-profile backup checksum mismatch'}
    $lines=@(Get-Content -LiteralPath $candidate | Where-Object{$_ -match '"event":"backup"'})
    if($lines.Count -ne 1){throw 'Recovery requires one complete original-profile backup'}
    $row=$lines[0]|ConvertFrom-Json
    if($row.hex -notmatch '^[0-9a-f]{1680}$' -or $row.address -notmatch '^[0-9a-f]{8}$'){throw 'Original-profile backup is incomplete'}
    return $candidate
}
try{
    if(-not (OwnedPath (Join-Path $SessionDir 'session.jsonl'))){throw 'Invalid session folder'}
    [IO.Directory]::CreateDirectory($SessionDir)|Out-Null
    Record 'session-started' @{mode=$Mode;durationSeconds=$DurationSeconds}
    if((Hash $engine) -ne ([IO.File]::ReadAllText($engine+'.sha256')).Trim()){throw 'Packaged engine checksum mismatch'}
    $sessionMutex=New-Object Threading.Mutex($false,'Global\FlydigiBridge.HardwareSession')
    try{$mutexHeld=$sessionMutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$mutexHeld=$true}
    if(-not $mutexHeld){throw 'Another application session is already running'}
    $legacy=@(Get-Process -Name FlydigiVirtualXboxBridge -ErrorAction SilentlyContinue)
    if($legacy.Count){throw 'The research bridge is still running. Stop it before starting this application'}
    $requirements=@()
    $preflightCode=Native -arguments @('--preflight')
    $requirements+=@{name='Virtual controller driver';ready=($preflightCode -eq 0);help='Install the WinUHid driver described in the setup guide'}
    $requirements+=@{name='Microsoft GameInput';ready=[IO.File]::Exists((Join-Path $env:SystemRoot 'System32\GameInputRedist.dll'));help='Install the official GameInput redistributable'}
    $cli=Join-Path ${env:ProgramFiles} 'Nefarius Software Solutions\HidHide\x64\HidHideCLI.exe'
    $requirements+=@{name='HidHide';ready=[IO.File]::Exists($cli);help='Install the official HidHide package and reconnect the dongle'}
    $physicalDriver=Join-Path $env:SystemRoot 'System32\drivers\Xbox360Filter64.sys'
    $driverReady=[IO.File]::Exists($physicalDriver) -and (Hash $physicalDriver) -eq '04f6e18f5b08bfee0411e2e75897b7dea74f9baf83148e84dc59516ebab58913'
    $requirements+=@{name='Flydigi transport driver';ready=$driverReady;help='The supported Space Station transport driver is required'}
    foreach($item in $requirements){Record 'requirement' $item}
    $missing=@($requirements|Where-Object{-not $_.ready})
    if($Mode -eq 'Requirements'){$nativeCode=0;Record 'requirements-checked' @{ready=($missing.Count -eq 0)};return}
    if($Mode -eq 'SetupExistingCheck'){
        foreach($component in @('WinUHid','GameInput','HidHide')){& (Join-Path $RuntimeRoot 'setup.ps1') -Component $component -RuntimeRoot $RuntimeRoot -DataRoot $DataRoot -LogPath $lifecycle -Install -ExistingOnly}
        $nativeCode=0;return
    }
    if($Mode -in @('SetupWinUHid','SetupHidHide','SetupGameInput','RemoveWinUHid','ValidateSetup')){
        $component=switch($Mode){'SetupWinUHid'{'WinUHid'};'SetupHidHide'{'HidHide'};'SetupGameInput'{'GameInput'};'RemoveWinUHid'{'RemoveWinUHid'};'ValidateSetup'{'Validate'}}
        & (Join-Path $RuntimeRoot 'setup.ps1') -Component $component -RuntimeRoot $RuntimeRoot -DataRoot $DataRoot -LogPath $lifecycle -Install:($Mode -ne 'ValidateSetup')
        $nativeCode=0;return
    }
    if($missing.Count){throw ('Setup is incomplete: '+(($missing|ForEach-Object{$_.name}) -join ', '))}
    $originalLog=$null
    if([IO.File]::Exists($pendingPath)){
        $pending=Get-Content -LiteralPath $pendingPath -Raw|ConvertFrom-Json
        $originalLog=PendingBackup $pending
    }
    if($Mode -eq 'Recover' -and -not $originalLog){throw 'No complete original-profile backup is available for recovery'}
    if($Mode -eq 'VirtualCheck' -or $Mode -in @('Bridge','Test')){
        $checks=Join-Path $DataRoot 'checks';[IO.Directory]::CreateDirectory($checks)|Out-Null
        $checkReceipt=Join-Path $checks ((Hash $engine)+'.json')
        $validReceipt=$false
        if([IO.File]::Exists($checkReceipt)){
            try{
                $receipt=Get-Content -LiteralPath $checkReceipt -Raw|ConvertFrom-Json
                $validReceipt=$receipt.owner -eq 'FlydigiBridge' -and $receipt.passed -eq $true -and $receipt.engineSha256 -eq (Hash $engine) -and (OwnedPath $receipt.log) -and [IO.File]::Exists($receipt.log) -and $receipt.logSha256 -eq (Hash $receipt.log)
            }catch{$validReceipt=$false}
        }
        if($Mode -eq 'VirtualCheck' -or -not $validReceipt){
            Record 'validating-virtual-controller' 'Keep the validation window active for a moment'
            $apiLog=Join-Path $SessionDir 'virtual-check.jsonl'
            $apiCode=Native -arguments @('--virtual-check','--log',$apiLog,'--stop-event',$StopEvent,'--parent-pid',[string]$ParentPid)
            $apiRows=@(Get-Content -LiteralPath $apiLog | ForEach-Object{$_|ConvertFrom-Json})
            if($apiCode -ne 0 -or -not @($apiRows|Where-Object{$_.virtualApiPassed -eq $true}).Count){
                $apiError=@($apiRows|Where-Object{$_.event -eq 'native-error'}|Select-Object -Last 1)
                throw ('Virtual controller validation failed. '+$(if($apiError.Count){$apiError[0].message}else{'Keep its window active and retry'}))
            }
            @{owner='FlydigiBridge';engineSha256=(Hash $engine);passed=$true;log=$apiLog;logSha256=(Hash $apiLog);utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath $checkReceipt -Encoding utf8
            Record 'virtual-controller-validated' 'All four output channels are available'
        }
        if($Mode -eq 'VirtualCheck'){$nativeCode=0;return}
    }
    if(Get-Process -Name 'Flydigi Space Station' -ErrorAction SilentlyContinue){throw 'Close Space Station, including its tray icon, before starting'}
    $service=Get-Service -Name $serviceName
    $restoreRunning=($service.Status -eq 'Running') -or ($pending -and $pending.restoreRunning)
    # The pending journal is created before any RAM changes; the native engine
    # flushes original-profile.jsonl to disk before sending a patch packet.
    if($Mode -ne 'Recover'){
        @{owner='FlydigiBridge';originalLog=$recoveryLog;fallbackLog=$originalLog;restoreRunning=[bool]$restoreRunning;utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json|Set-Content -LiteralPath $pendingPath -Encoding utf8
    }
    $serviceTouched=$true
    if($service.Status -eq 'Running'){
        Record 'pausing-service' 'Preparing the controller connection'
        $oldIds=@(Get-Process -Name SpaceStationService -ErrorAction SilentlyContinue|Select-Object -ExpandProperty Id)
        Stop-Service -InputObject $service;$service.WaitForStatus('Stopped',[TimeSpan]::FromSeconds(20))
        $deadline=[DateTime]::UtcNow.AddSeconds(20)
        do{$remaining=@($oldIds|ForEach-Object{Get-Process -Id $_ -ErrorAction SilentlyContinue});if(-not $remaining.Count){break};Start-Sleep -Milliseconds 200}while([DateTime]::UtcNow -lt $deadline)
        foreach($process in $remaining){
            $service.Refresh();$expected=Join-Path ${env:ProgramFiles} 'Flydigi Space Station\SpaceStationService.exe'
            if($service.Status -ne 'Stopped' -or $process.ProcessName -ne 'SpaceStationService' -or $process.Path -ne $expected){throw 'Residual service process identity mismatch'}
            Stop-Process -Id $process.Id -Force
        }
        Start-Sleep -Seconds 2
    }
    $service.Refresh();if($service.Status -ne 'Stopped'){throw 'The Space Station service is not stopped'}
    $hidingReceipt=Join-Path $DataRoot 'hiding\active_session.json'
    if([IO.File]::Exists($hidingReceipt)){
        $oldHiding=Get-Content -LiteralPath $hidingReceipt -Raw|ConvertFrom-Json
        if($oldHiding.state -ne 'restored'){& (Join-Path $RuntimeRoot 'hidhide-session.ps1') -Restore -RuntimeRoot $RuntimeRoot -DataRoot $DataRoot}
    }
    if($Mode -ne 'Recover'){
        $hidingTouched=$true;& (Join-Path $RuntimeRoot 'hidhide-session.ps1') -Enable -RuntimeRoot $RuntimeRoot -DataRoot $DataRoot
        Record 'physical-controller-hidden' 'Only the virtual Xbox controller is exposed to games'
    }
    Record 'connecting-controller' 'Switch on the controller through the dongle'
    $nativeArguments=@($(if($Mode -eq 'Test'){'--bridge-check'}elseif($Mode -eq 'Recover'){'--restore-profile'}else{'--bridge'}),'--log',$nativeLog,'--stop-event',$StopEvent,'--parent-pid',[string]$ParentPid)
    if($Mode -eq 'Bridge'){$nativeArguments+=@('--duration-seconds',[string]$DurationSeconds)}
    if($Mode -ne 'Recover'){$nativeArguments+=@('--recovery-file',$recoveryLog)}
    if($originalLog){$nativeArguments+=@('--resume-backup',$originalLog)}
    if($DiagnosticLogging){$nativeArguments+='--diagnostic-logging'}
    $nativeCode=Native -arguments $nativeArguments
    Record 'native-exit' @{code=$nativeCode}
}catch{$errorText=$_.Exception.Message;Record 'session-error' $errorText}
finally{
    $restored=$false;$visibilityRestored=$true;$serviceRestored=$false
    if($hidingTouched){
        try{& (Join-Path $RuntimeRoot 'hidhide-session.ps1') -Restore -RuntimeRoot $RuntimeRoot -DataRoot $DataRoot;Record 'visibility-restored' 'Original controller visibility restored'}
        catch{$visibilityRestored=$false;Record 'session-error' ('Visibility restoration failed: '+$_.Exception.Message)}
    }
    if($physical -and $serviceTouched){
        if([IO.File]::Exists($nativeLog)){
            $finish=@(Get-Content -LiteralPath $nativeLog | Where-Object{$_ -match '"event":"finish"'} | ForEach-Object{$_|ConvertFrom-Json})
            $restored=@($finish|Where-Object{$_.restored840 -eq $true -or $_.profilePatchAttempted -eq $false}).Count -gt 0
        }
        if(-not [IO.File]::Exists($recoveryLog) -and -not $originalLog -and $Mode -ne 'Recover'){$restored=$true}
        if($restored){
            Record 'profile-restored' 'Original profile restored and verified'
            try{if($restoreRunning){Start-Service -Name $serviceName;$service=Get-Service $serviceName;$service.WaitForStatus('Running',[TimeSpan]::FromSeconds(20))};$serviceRestored=$true;Record 'service-restored' 'Previous service state restored'}
            catch{Record 'session-error' ('Service restart failed: '+$_.Exception.Message)}
            if($serviceRestored -and $visibilityRestored -and [IO.File]::Exists($pendingPath)){Remove-Item -LiteralPath $pendingPath}
        }else{
            if([IO.File]::Exists($pendingPath)){
                $receipt=Get-Content -LiteralPath $pendingPath -Raw|ConvertFrom-Json
                if([IO.File]::Exists($receipt.originalLog)){$receipt|Add-Member -NotePropertyName sha256 -NotePropertyValue (Hash $receipt.originalLog) -Force}
                $receipt|ConvertTo-Json|Set-Content -LiteralPath $pendingPath -Encoding utf8
            }
            Record 'recovery-required' 'Switch on the same controller and use Restore profile. Keep Space Station closed'
        }
    }elseif(-not $serviceTouched){$restored=$true;$serviceRestored=$true}
    Record 'session-finished' @{code=$nativeCode;profileRestored=$restored;visibilityRestored=$visibilityRestored;serviceRestored=$serviceRestored;needsRecovery=($physical -and $serviceTouched -and (-not $restored -or -not $serviceRestored -or -not $visibilityRestored));error=$errorText}
    if($mutexHeld){$sessionMutex.ReleaseMutex()};if($sessionMutex){$sessionMutex.Dispose()}
}
exit $nativeCode
