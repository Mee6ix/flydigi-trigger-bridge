[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SdkRoot,[Parameter(Mandatory=$true)][string]$WdkRoot,[string]$KitVersion='10.0.26100.0')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root 'third_party\WinUHid\WinUHid Driver'
$work=Join-Path $root 'build\driver';$package=Join-Path $root 'drivers\WinUHid'
[IO.Directory]::CreateDirectory($work)|Out-Null;[IO.Directory]::CreateDirectory($package)|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath|Select-Object -First 1
if(-not $vs){throw 'Visual Studio C++ Build Tools are required'}
$sdkTools=Join-Path $SdkRoot ('bin\'+$KitVersion+'\x86')
$signTool=Join-Path $sdkTools 'signtool.exe'
$inf2cat=Join-Path $WdkRoot ('bin\'+$KitVersion+'\x86\Inf2Cat.exe')
foreach($tool in @($signTool,$inf2cat,(Join-Path $sdkTools 'tracewpp.exe'))){if(-not [IO.File]::Exists($tool)){throw ('Missing SDK/WDK tool: '+$tool)}}
$driverSource=Join-Path $work 'WinUHid.c'
$text=[IO.File]::ReadAllText((Join-Path $source 'WinUHid.c'))
$text="#define WIN32_NO_STATUS`r`n#include <windows.h>`r`n#undef WIN32_NO_STATUS`r`n"+$text
$text=$text.Replace('WPP_INIT_TRACING(DriverObject, RegistryPath);','WPP_INIT_TRACING(L"FlydigiBridgeWinUHid");').Replace('WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));','WPP_CLEANUP();').Replace('WPP_CLEANUP(DriverObject);','WPP_CLEANUP();')
[IO.File]::WriteAllText($driverSource,$text,[Text.Encoding]::ASCII)
& (Join-Path $sdkTools 'tracewpp.exe') '-um' '-dll' ('-cfgdir:'+(Join-Path $SdkRoot ('bin\'+$KitVersion+'\WppConfig\Rev1'))) ('-scan:'+(Join-Path $source 'Trace.h')) ('-odir:'+$work) $driverSource
if($LASTEXITCODE -ne 0){throw 'Trace preprocessing failed'}
$includes=@((Join-Path $WdkRoot 'Include\wdf\umdf\2.15'),(Join-Path $SdkRoot ('Include\'+$KitVersion+'\shared')),(Join-Path $SdkRoot ('Include\'+$KitVersion+'\um')),$source,$work)
$libs=@((Join-Path $WdkRoot 'Lib\wdf\umdf\x64\2.15'),(Join-Path $WdkRoot ('Lib\'+$KitVersion+'\um\x64')))
$dll=Join-Path $package 'WinUHidDriver.dll';$helper=Join-Path $package 'DriverDevice.exe'
$commands=@('@echo off',('call "'+(Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat')+'" >nul'),'if errorlevel 1 exit /b 1')
$commands+='cl /nologo /LD /TC /MD /W4 /O2 /guard:cf /DUMDF_VERSION_MAJOR=2 /DUMDF_VERSION_MINOR=15 /D_WIN32_WINNT=0x0A00 '+(($includes|ForEach-Object{'/I"'+$_+'"'}) -join ' ')+' "'+$driverSource+'" /Fo:"'+(Join-Path $work 'driver.obj')+'" /Fe:"'+$dll+'" /link /IMPLIB:"'+(Join-Path $work 'WinUHidDriver.lib')+'" '+(($libs|ForEach-Object{'/libpath:"'+$_+'"'}) -join ' ')+' /export:DriverEntry WdfDriverStubUm.lib VhfUm.lib ntdll.lib advapi32.lib /guard:cf /CETCOMPAT'
$commands+='if errorlevel 1 exit /b 1'
$commands+='cl /nologo /std:c++17 /EHsc /MT /O2 /guard:cf "'+(Join-Path $root 'src\Driver\device.cpp')+'" /Fo:"'+(Join-Path $work 'device.obj')+'" /Fe:"'+$helper+'" /link setupapi.lib newdev.lib cfgmgr32.lib /guard:cf /CETCOMPAT'
$commands+='exit /b %errorlevel%'
$compile=Join-Path $work 'compile.cmd';[IO.File]::WriteAllText($compile,($commands -join [Environment]::NewLine),[Text.Encoding]::ASCII)
& $env:ComSpec /d /c $compile;if($LASTEXITCODE -ne 0){throw 'Driver/helper build failed'}
$inf=[IO.File]::ReadAllText((Join-Path $source 'WinUHidDriver.inf')).Replace('NT$ARCH$','NTamd64').Replace('$UMDFVERSION$','2.15.0').Replace('DriverVer   = ; TODO: set DriverVer in stampinf property pages','DriverVer = 10/08/2026,1.0.0.2').Replace('ManufacturerName = "WinUHid Project"','ManufacturerName = "Flydigi Bridge Driver"')
if($inf.Contains('$ARCH$') -or $inf.Contains('$UMDFVERSION$') -or $inf.Contains('(A;;GA;;;WD)')){throw 'Invalid driver template/access policy'}
[IO.File]::WriteAllText((Join-Path $package 'WinUHidDriver.inf'),$inf,[Text.Encoding]::ASCII)
Copy-Item -LiteralPath (Join-Path $root 'third_party\WinUHid\LICENSE') -Destination (Join-Path $package 'LICENSE') -Force
$generator=Join-Path $work 'NewDriverCertificate.exe'
& "$env:SystemRoot\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:exe /platform:x64 ('/out:'+$generator) (Join-Path $PSScriptRoot 'NewDriverCertificate.cs')
if($LASTEXITCODE -ne 0){throw 'Offline certificate tool build failed'}
$pfx=Join-Path $work 'temporary-signing-key.pfx';$passwordFile=Join-Path $work 'temporary-password.txt';$publicCert=Join-Path $package 'FlydigiBridgeDriver.cer'
$random=New-Object byte[] 32;$rng=[Security.Cryptography.RandomNumberGenerator]::Create();try{$rng.GetBytes($random)}finally{$rng.Dispose()};$password=[Convert]::ToBase64String($random)
try{
    [IO.File]::WriteAllText($passwordFile,$password)
    & $generator $pfx $publicCert $passwordFile;if($LASTEXITCODE -ne 0){throw 'Offline certificate generation failed'}
    & $signTool sign /fd SHA256 /f $pfx /p $password $dll;if($LASTEXITCODE -ne 0){throw 'Driver signing failed'}
    & $inf2cat ('/driver:'+$package) '/os:10_X64' '/uselocaltime';if($LASTEXITCODE -ne 0){throw 'Catalog build failed'}
    & $signTool sign /fd SHA256 /f $pfx /p $password (Join-Path $package 'winuhiddriver.cat');if($LASTEXITCODE -ne 0){throw 'Catalog signing failed'}
}finally{foreach($private in @($pfx,$passwordFile)){if([IO.File]::Exists($private)){[IO.File]::Delete($private)}};$password=$null}
$cert=New-Object Security.Cryptography.X509Certificates.X509Certificate2($publicCert)
$files=@{};foreach($name in @('WinUHidDriver.dll','WinUHidDriver.inf','winuhiddriver.cat','DriverDevice.exe','FlydigiBridgeDriver.cer','LICENSE')){$files[$name]=(Get-FileHash -LiteralPath (Join-Path $package $name)).Hash.ToLowerInvariant()}
@{owner='FlydigiBridge';version='1.0.0.2';instanceId='ROOT\WINUHID\FLYDIGI_BRIDGE';sourceCommit='d6cebbef5c7909168d1f881185be8f607d6aefd4';subject=$cert.Subject;thumbprint=$cert.Thumbprint;expiresUtc=$cert.NotAfter.ToUniversalTime().ToString('o');files=$files;selfSigned=$true;installed=$false}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $package 'package.json') -Encoding utf8
Write-Host 'Driver payload built. Only the public certificate ships; the temporary private key was deleted. No certificate trust, driver install or Windows security setting was changed.'
