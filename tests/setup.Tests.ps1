$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$fixture=Join-Path $root ('build\setup-tests\'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture)|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'drivers') -Destination (Join-Path $fixture 'drivers') -Recurse
$log=Join-Path $fixture 'events.jsonl';$data=Join-Path $fixture 'data'
$scriptPath=Join-Path $root 'runtime\setup.ps1'
& $scriptPath -Component Validate -RuntimeRoot $fixture -DataRoot $data -LogPath $log
$dll=Join-Path $fixture 'drivers\WinUHid\WinUHidDriver.dll';$manifestPath=Join-Path $fixture 'drivers\WinUHid\package.json'
$original=[IO.File]::ReadAllBytes($dll);$manifestText=[IO.File]::ReadAllText($manifestPath)
$modified=$original.Clone();$modified[1024]=$modified[1024] -bxor 1;[IO.File]::WriteAllBytes($dll,$modified)
$manifest=$manifestText|ConvertFrom-Json;$manifest.files.'WinUHidDriver.dll'=(Get-FileHash $dll).Hash.ToLowerInvariant();$manifest|ConvertTo-Json -Depth 5|Set-Content $manifestPath -Encoding utf8
$rejected=$false;try{& $scriptPath -Component Validate -RuntimeRoot $fixture -DataRoot $data -LogPath $log}catch{if($_.Exception.Message -match 'signer or cryptographic signature mismatch'){$rejected=$true}else{throw}}
if(-not $rejected){throw 'Modified driver with a matching updated manifest bypassed cryptographic verification'}
[IO.File]::WriteAllBytes($dll,$original);[IO.File]::WriteAllText($manifestPath,$manifestText)
$rejected=$false;try{& $scriptPath -Component WinUHid -RuntimeRoot $fixture -DataRoot $data -LogPath $log}catch{if($_.Exception.Message -match 'explicit user-confirmed'){$rejected=$true}else{throw}}
if(-not $rejected){throw 'Setup did not require explicit installation intent'}
if(@(Get-ChildItem -LiteralPath (Join-Path $root 'drivers') -Recurse -File|Where-Object{$_.Extension -in @('.pfx','.p12','.key')}).Count){throw 'A private signing key was included'}
Write-Host 'Installer tests passed: original signatures, tamper rejection beyond manifest hashes, explicit install intent and no bundled private key. No drivers, certificates or controller settings changed.'
