[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet('WinUHid','HidHide','GameInput','RemoveWinUHid','Validate')][string]$Component,
      [Parameter(Mandatory=$true)][string]$RuntimeRoot,[Parameter(Mandatory=$true)][string]$DataRoot,
      [Parameter(Mandatory=$true)][string]$LogPath,[switch]$Install,[switch]$ExistingOnly)
$ErrorActionPreference='Stop'
function SetupRecord($event,$detail){@{event=$event;utc=[DateTime]::UtcNow.ToString('o');detail=$detail}|ConvertTo-Json -Depth 5 -Compress|Add-Content -LiteralPath $LogPath -Encoding utf8}
function Hash($file){(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()}
$drivers=Join-Path $RuntimeRoot 'drivers';$packageRoot=Join-Path $drivers 'WinUHid'
$manifest=Get-Content -LiteralPath (Join-Path $packageRoot 'package.json') -Raw|ConvertFrom-Json
$installers=Get-Content -LiteralPath (Join-Path $drivers 'installers.json') -Raw|ConvertFrom-Json
$receiptPath=Join-Path $DataRoot 'installation\winuhid.json'
$certPath=Join-Path $packageRoot 'FlydigiBridgeDriver.cer'
$cert=New-Object Security.Cryptography.X509Certificates.X509Certificate2($certPath)
$helper=Join-Path $packageRoot 'DriverDevice.exe'
if(-not ('FlydigiDriverSignature' -as [type])){Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FlydigiDriverSignature {
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]private struct FileInfo{public uint Size;[MarshalAs(UnmanagedType.LPWStr)]public string Path;public IntPtr File,KnownSubject;}
    [StructLayout(LayoutKind.Sequential)]private struct Data{public uint Size;public IntPtr Policy,Sip;public uint UI,Revocation,Choice;public IntPtr File;public uint StateAction;public IntPtr State,Url;public uint Flags,Context;public IntPtr SignatureSettings;}
    [DllImport("wintrust.dll",ExactSpelling=true)]private static extern int WinVerifyTrust(IntPtr window,ref Guid action,ref Data data);
    public static bool ValidOrUntrustedRoot(string path){
        var file=new FileInfo{Size=(uint)Marshal.SizeOf(typeof(FileInfo)),Path=path};IntPtr pointer=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(FileInfo)));Marshal.StructureToPtr(file,pointer,false);
        try{var data=new Data{Size=(uint)Marshal.SizeOf(typeof(Data)),UI=2,Choice=1,File=pointer,Flags=0x1010};var action=new Guid("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");int result=WinVerifyTrust(new IntPtr(-1),ref action,ref data);return result==0||result==unchecked((int)0x800B0109);}
        finally{Marshal.DestroyStructure(pointer,typeof(FileInfo));Marshal.FreeHGlobal(pointer);}
    }
}
'@
}
function VerifyDriver{
    if($manifest.owner -ne 'FlydigiBridge' -or $manifest.instanceId -ne 'ROOT\WINUHID\FLYDIGI_BRIDGE'){throw 'Unexpected driver package identity'}
    $expected=@('WinUHidDriver.dll','WinUHidDriver.inf','winuhiddriver.cat','DriverDevice.exe','FlydigiBridgeDriver.cer','LICENSE')
    if(@($manifest.files.PSObject.Properties).Count -ne $expected.Count){throw 'Unexpected driver package file list'}
    foreach($name in $expected){if((Hash (Join-Path $packageRoot $name)) -ne $manifest.files.$name){throw ('Driver package checksum mismatch: '+$name)}}
    if($cert.HasPrivateKey -or $cert.Subject -ne 'CN=Flydigi Bridge Driver' -or $cert.Thumbprint -ne $manifest.thumbprint -or $cert.Issuer -ne $cert.Subject -or $cert.NotAfter -lt [DateTime]::Now){throw 'Driver public certificate identity/expiry mismatch'}
    $eku=@($cert.Extensions|Where-Object{$_.Oid.Value -eq '2.5.29.37'})
    if($eku.Count -ne 1 -or @($eku[0].EnhancedKeyUsages).Count -ne 1 -or $eku[0].EnhancedKeyUsages[0].Value -ne '1.3.6.1.5.5.7.3.3'){throw 'Driver certificate must be code-signing only'}
    foreach($file in @('WinUHidDriver.dll','winuhiddriver.cat')){
        $signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $packageRoot $file)
        if(-not $signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint -or -not [FlydigiDriverSignature]::ValidOrUntrustedRoot((Join-Path $packageRoot $file))){throw 'Driver package signer or cryptographic signature mismatch'}
    }
}
function VerifyInstaller($name){
    $item=$installers.$name
    $allow=@{HidHide='HidHide/HidHide_1.5.230_x64.exe';GameInput='GameInput/GameInputRedist.msi';VCRuntime='VCRuntime/vc_redist.x64.exe'}
    if(-not $allow.ContainsKey($name) -or $item.file -ne $allow[$name]){throw 'Unexpected prerequisite path'}
    $file=Join-Path $drivers $item.file
    if((Hash $file) -ne $item.sha256){throw ('Prerequisite checksum mismatch: '+$name)}
    $signature=Get-AuthenticodeSignature -LiteralPath $file
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $item.thumbprint){throw ('Prerequisite signature mismatch: '+$name)}
    return $file
}
function RunInstaller($file,$arguments){
    $process=Start-Process -FilePath $file -ArgumentList $arguments -WindowStyle Normal -Wait -PassThru
    if($process.ExitCode -notin @(0,3010)){throw ('Installation did not complete: exit '+$process.ExitCode)}
    if($process.ExitCode -eq 3010){SetupRecord 'setup-restart-required' 'Restart Windows before using the bridge. This application does not restart Windows automatically.'}
}
function SaveReceipt($receipt){$receipt|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $receiptPath -Encoding utf8}
VerifyDriver
if($Component -eq 'Validate'){
    foreach($name in @('HidHide','GameInput','VCRuntime')){VerifyInstaller $name|Out-Null}
    SetupRecord 'setup-package-validated' 'Bundled driver hashes, public certificate, signers and official installers verified. No Windows changes.'
    return
}
if(-not $Install){throw 'Installation requires an explicit user-confirmed request'}
if(-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator access is required'}
if([IO.File]::Exists((Join-Path $DataRoot 'pending-restore.json'))){throw 'Restore the controller profile before installing or removing drivers'}
if(Get-Process -Name FlydigiBridgeEngine,FlydigiVirtualXboxBridge -ErrorAction SilentlyContinue){throw 'Stop the bridge before changing drivers'}
switch($Component){
    'HidHide'{
        if(Test-Path -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Services\HidHide'){SetupRecord 'setup-skipped' 'HidHide already exists. The existing installation was kept.';return}
        if($ExistingOnly){throw 'Read-only skip check: HidHide is absent. No installation was attempted.'}
        $file=VerifyInstaller 'HidHide';SetupRecord 'setup-installing' 'Installing the official HidHide filter. No hiding rules will be changed.'
        RunInstaller $file @('/exenoui','/passive','/norestart')
        if(-not (Test-Path -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Services\HidHide')){throw 'HidHide service was not found after setup'}
    }
    'GameInput'{
        if([IO.File]::Exists((Join-Path $env:SystemRoot 'System32\GameInputRedist.dll'))){SetupRecord 'setup-skipped' 'GameInput already exists. The existing installation was kept.';return}
        if($ExistingOnly){throw 'Read-only skip check: GameInput is absent. No installation was attempted.'}
        $file=VerifyInstaller 'GameInput';SetupRecord 'setup-installing' 'Installing the official Microsoft GameInput redistributable.'
        RunInstaller (Join-Path $env:SystemRoot 'System32\msiexec.exe') @('/i',('"'+$file+'"'),'/norestart')
        if(-not [IO.File]::Exists((Join-Path $env:SystemRoot 'System32\GameInputRedist.dll'))){throw 'GameInput was not found after setup'}
    }
    'WinUHid'{
        & (Join-Path $RuntimeRoot 'FlydigiBridgeEngine.exe') --preflight|Out-Null
        if($LASTEXITCODE -eq 0){SetupRecord 'setup-skipped' 'A working WinUHid driver already exists. No driver or certificate was changed.';return}
        if($ExistingOnly){throw 'Read-only skip check: WinUHid is absent. No installation was attempted.'}
        if([IO.File]::Exists($receiptPath)){
            $previous=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
            if($previous.owner -ne 'FlydigiBridge' -or $previous.state -ne 'uninstalled' -or $previous.thumbprint -ne $cert.Thumbprint){throw 'An owned installation receipt already exists. Review or remove that installation first.'}
        }
        & $helper --check-absent|Out-Null;if($LASTEXITCODE -ne 0){throw 'Another WinUHid root device exists. It will not be replaced.'}
        if(-not [IO.File]::Exists((Join-Path $env:SystemRoot 'System32\vcruntime140.dll'))){
            $file=VerifyInstaller 'VCRuntime';SetupRecord 'setup-installing' 'Microsoft Visual C++ x64 runtime is required. Complete its official installer.'
            RunInstaller $file @('/install','/norestart')
            if(-not [IO.File]::Exists((Join-Path $env:SystemRoot 'System32\vcruntime140.dll'))){throw 'The Visual C++ x64 runtime is not available'}
        }
        [IO.Directory]::CreateDirectory((Split-Path $receiptPath -Parent))|Out-Null
        $receipt=[ordered]@{owner='FlydigiBridge';instanceId=$manifest.instanceId;thumbprint=$cert.Thumbprint;infSha256=$manifest.files.'WinUHidDriver.inf';publishedInf=$null;rootAdded=$false;publisherAdded=$false;state='starting';utc=[DateTime]::UtcNow.ToString('o')}
        SaveReceipt $receipt
        try{
            foreach($store in @('Root','TrustedPublisher')){
                $field=if($store -eq 'Root'){'rootAdded'}else{'publisherAdded'}
                if(-not (Test-Path -LiteralPath ('Cert:\LocalMachine\'+$store+'\'+$cert.Thumbprint))){
                    # Journal intent before the persistent trust operation.
                    $receipt[$field]=$true;SaveReceipt $receipt
                    Import-Certificate -FilePath $certPath -CertStoreLocation ('Cert:\LocalMachine\'+$store)|Out-Null
                }
            }
            foreach($file in @('WinUHidDriver.dll','winuhiddriver.cat')){if((Get-AuthenticodeSignature -LiteralPath (Join-Path $packageRoot $file)).Status -ne 'Valid'){throw 'Driver signature is not valid after explicit trust'}}
            SetupRecord 'setup-installing' 'Installing the bundled WinUHid virtual controller driver.'
            $text=@(& $helper --create (Join-Path $packageRoot 'WinUHidDriver.inf'));if($LASTEXITCODE -ne 0){throw 'Driver binding failed. The installation receipt was retained.'}
            $result=($text -join [Environment]::NewLine)|ConvertFrom-Json
            $receipt.publishedInf=$result.publishedInf;$receipt.state='installed';SaveReceipt $receipt
            if($result.rebootRequired){SetupRecord 'setup-restart-required' 'Restart Windows to load the driver. No automatic restart was requested.'}
            else{& (Join-Path $RuntimeRoot 'FlydigiBridgeEngine.exe') --preflight|Out-Null;if($LASTEXITCODE -ne 0){throw 'Driver installed, but is not ready. Restart Windows and check setup.'}}
        }catch{$receipt.state='needs-review';SaveReceipt $receipt;throw}
    }
    'RemoveWinUHid'{
        if($ExistingOnly){throw 'Read-only checks cannot remove drivers.'}
        if(-not [IO.File]::Exists($receiptPath)){throw 'No installation owned by this application was recorded. Existing drivers will not be removed.'}
        $receipt=Get-Content -LiteralPath $receiptPath -Raw|ConvertFrom-Json
        if($receipt.owner -ne 'FlydigiBridge' -or $receipt.instanceId -ne $manifest.instanceId -or $receipt.thumbprint -ne $cert.Thumbprint -or $receipt.infSha256 -ne $manifest.files.'WinUHidDriver.inf'){throw 'Installation receipt does not match this package'}
        if($receipt.publishedInf){
            if($receipt.publishedInf -notmatch '^oem[0-9]+\.inf$'){throw 'Invalid installed INF identity'}
            $installedInf=Join-Path (Join-Path $env:SystemRoot 'INF') $receipt.publishedInf
            if([IO.File]::Exists($installedInf) -and (Hash $installedInf) -ne $receipt.infSha256){throw 'The installed driver changed. Removal was refused.'}
        }
        & $helper --remove|Out-Null;if($LASTEXITCODE -ne 0){throw 'Owned node removal failed. Certificate trust was retained.'}
        if($receipt.publishedInf -and [IO.File]::Exists($installedInf)){& (Join-Path $env:SystemRoot 'System32\pnputil.exe') /delete-driver $receipt.publishedInf|Out-Null;if($LASTEXITCODE -ne 0){throw 'Owned package removal failed. Certificate trust was retained.'}}
        foreach($store in @('Root','TrustedPublisher')){
            $added=if($store -eq 'Root'){$receipt.rootAdded}else{$receipt.publisherAdded}
            $path='Cert:\LocalMachine\'+$store+'\'+$receipt.thumbprint
            if($added -and (Test-Path -LiteralPath $path)){if((Get-Item -LiteralPath $path).Subject -ne 'CN=Flydigi Bridge Driver'){throw 'Certificate identity changed'};Remove-Item -LiteralPath $path}
        }
        $receipt.state='uninstalled';SaveReceipt $receipt
    }
}
SetupRecord 'setup-complete' 'Setup completed. Check setup before starting the bridge.'
