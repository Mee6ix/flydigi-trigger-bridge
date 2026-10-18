[CmdletBinding()]
param([switch]$SkipTests,[switch]$RuntimeOnly,[switch]$AppOnly)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$build=Join-Path $root 'build';$dist=Join-Path $root 'dist'
[IO.Directory]::CreateDirectory($build)|Out-Null;[IO.Directory]::CreateDirectory($dist)|Out-Null
$cache=Join-Path $root '.cache';[IO.Directory]::CreateDirectory($cache)|Out-Null
$dependencies=Get-Content -LiteralPath (Join-Path $root 'dependencies.json') -Raw|ConvertFrom-Json
$header=Join-Path $cache 'GameInput.h'
if(-not [IO.File]::Exists($header)){Invoke-WebRequest -UseBasicParsing -Uri $dependencies.gameInputHeader.url -OutFile $header}
if((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dependencies.gameInputHeader.sha256){throw 'Pinned GameInput SDK header checksum mismatch'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not [IO.File]::Exists($vswhere)){throw 'Visual Studio Build Tools with the C++ desktop workload are required'}
$visualStudio=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if(-not $visualStudio){throw 'MSVC x64 compiler is not installed'}
$vcvars=Join-Path $visualStudio 'VC\Auxiliary\Build\vcvars64.bat'
$vendor=Join-Path $root 'third_party\WinUHid'
$sources=@((Join-Path $root 'src\Native\virtual_xbox_bridge.cpp'),(Join-Path $vendor 'WinUHid\WinUHid.cpp'),(Join-Path $vendor 'WinUHidDevs\WinUHidDevs.cpp'),(Join-Path $vendor 'WinUHidDevs\WinUHidXOne.cpp'))
$commands=@('@echo off',('call "'+$vcvars+'" >nul'),'if errorlevel 1 exit /b 1');$objects=@()
for($i=0;$i -lt $sources.Count;$i++){
    $object=Join-Path $build ('native_'+$i+'.obj');$objects+=$object
    $define=if($i -eq 1){' /DDllMain=UnusedWinUHidCoreDllMain'}elseif($i -eq 2){' /DDllMain=UnusedWinUHidPresetDllMain'}else{''}
    $commands+='cl /nologo /c /std:c++17 /EHsc /MT /W4 /O2 /DWINUHID_STATIC /D_WIN32_WINNT=0x0A00'+$define+' /I"'+$cache+'" /I"'+(Join-Path $vendor 'WinUHid')+'" /I"'+(Join-Path $vendor 'WinUHidDevs')+'" "'+$sources[$i]+'" /Fo:"'+$object+'"'
    $commands+='if errorlevel 1 exit /b 1'
}
$engine=Join-Path $build 'FlydigiBridgeEngine.exe'
$commands+='link /nologo '+(($objects|ForEach-Object{'"'+$_+'"'}) -join ' ')+' /out:"'+$engine+'" cfgmgr32.lib fltlib.lib advapi32.lib user32.lib'
$commands+='exit /b %errorlevel%'
$compile=Join-Path $build 'compile-native.cmd';[IO.File]::WriteAllText($compile,($commands -join [Environment]::NewLine),[Text.Encoding]::ASCII)
if(-not $AppOnly){& $env:ComSpec /d /c $compile;if($LASTEXITCODE -ne 0){throw 'Native engine build failed'}}
elseif(-not [IO.File]::Exists($engine)){throw 'AppOnly requires an existing native build'}
elseif(@(Get-ChildItem -LiteralPath (Join-Path $root 'src\Native') -File|Where-Object{$_.LastWriteTimeUtc -gt (Get-Item $engine).LastWriteTimeUtc}).Count){throw 'Native source changed. Run a full build instead'}
if(-not $SkipTests){& $engine --self-test;if($LASTEXITCODE -ne 0){throw 'Native offline checks failed'};& (Join-Path $root 'tests\setup.Tests.ps1')}
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$payload=Join-Path $build 'runtime.zip'
if([IO.File]::Exists($payload)){[IO.File]::Delete($payload)}
$archive=[IO.Compression.ZipFile]::Open($payload,[IO.Compression.ZipArchiveMode]::Create)
try{
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$engine,'FlydigiBridgeEngine.exe')|Out-Null
    $entry=$archive.CreateEntry('FlydigiBridgeEngine.exe.sha256');$writer=New-Object IO.StreamWriter($entry.Open());try{$writer.Write((Get-FileHash $engine).Hash.ToLowerInvariant())}finally{$writer.Dispose()}
    foreach($name in @('session.ps1','hidhide-session.ps1','setup.ps1')){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $root ('runtime\'+$name)),$name)|Out-Null}
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'drivers') -Recurse -File){
        if($file.Extension -in @('.pfx','.key','.p12','.obj','.lib','.exp','.pdb')){throw 'Private or development file found in driver payload'}
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$file.FullName.Substring($root.Length+1).Replace('\','/'))|Out-Null
    }
    foreach($name in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','docs\SETUP.md','docs\DRIVER.md','docs\ARCHITECTURE.md','docs\VALIDATION.md')){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $root $name),$name.Replace('\','/'))|Out-Null}
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $vendor 'LICENSE'),'licenses/WinUHid-LICENSE.txt')|Out-Null
}finally{$archive.Dispose()}
if($RuntimeOnly){Write-Host ('Runtime built: '+$payload);return}
& (Join-Path $root 'tools\make-icon.ps1') -OutputPath (Join-Path $build 'FlydigiTriggerBridge.ico')
$csc=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(-not [IO.File]::Exists($csc)){throw '.NET Framework 4.8 compiler is required'}
$application=Join-Path $dist 'FlydigiTriggerBridge.exe'
$compilerArguments=@('/nologo','/target:winexe','/platform:x64','/optimize+','/langversion:5',('/out:'+$application),('/win32icon:'+(Join-Path $build 'FlydigiTriggerBridge.ico')),('/win32manifest:'+(Join-Path $root 'src\App\app.manifest')),('/resource:'+$payload+',FlydigiTriggerBridge.Runtime.zip'),'/reference:System.dll','/reference:System.Core.dll','/reference:System.Drawing.dll','/reference:System.Windows.Forms.dll','/reference:System.Web.Extensions.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll')
$compilerArguments+=@(Get-ChildItem -LiteralPath (Join-Path $root 'src\App') -Filter '*.cs'|Select-Object -ExpandProperty FullName)
& $csc @compilerArguments;if($LASTEXITCODE -ne 0){throw 'Desktop application build failed'}
if(-not $SkipTests){$test=Start-Process -FilePath $application -ArgumentList '--self-test' -WindowStyle Hidden -PassThru -Wait;if($test.ExitCode -ne 0){throw 'Application workflow tests failed'}}
(Get-FileHash -LiteralPath $application -Algorithm SHA256).Hash.ToLowerInvariant()|Set-Content -LiteralPath (Join-Path $dist 'FlydigiTriggerBridge.exe.sha256') -Encoding ascii
foreach($name in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md')){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $dist -Force}
Write-Host ('Build complete: '+$application)
Write-Host 'Tests passed. No drivers were installed and no controller commands were sent.'
