[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root 'dist'}
$exe=Join-Path $root 'dist\FlydigiTriggerBridge.exe'
if(-not [IO.File]::Exists($exe)){throw 'Build the application with build.ps1 first'}
if((Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant() -ne ([IO.File]::ReadAllText($exe+'.sha256')).Trim()){throw 'Application checksum mismatch'}
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
function Archive($name,$files){
    $path=Join-Path $OutputDirectory $name
    if([IO.File]::Exists($path)){[IO.File]::Delete($path)}
    $zip=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Create)
    try{foreach($item in $files){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$item.path,$item.entry)|Out-Null}}
    finally{$zip.Dispose()}
    (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()|Set-Content -LiteralPath ($path+'.sha256') -Encoding ascii
    Write-Host ('Packaged: '+$path)
}
$source=@()
foreach($name in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','dependencies.json','build.ps1','package.ps1','FlydigiTriggerBridge.sln','.gitignore','.gitattributes')){
    $source+=@{path=(Join-Path $root $name);entry=('FlydigiTriggerBridge/'+$name)}
}
foreach($folder in @('src','runtime','tools','docs','third_party','.github','drivers','tests')){
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $root $folder) -File -Recurse){
        $relative=$file.FullName.Substring($root.Length+1).Replace('\','/')
        if($relative -match '(^|/)(bin|obj|\.vs)(/|$)' -or $file.Extension -in @('.pfx','.p12','.key','.jsonl','.bin','.obj','.pdb','.lib','.exp')){throw ('Unexpected generated/private source file: '+$relative)}
        if($file.Extension -in @('.exe','.dll','.sys','.cat','.cer','.msi') -and $folder -ne 'drivers'){throw ('Unexpected executable outside the documented driver payload: '+$relative)}
        $source+=@{path=$file.FullName;entry=('FlydigiTriggerBridge/'+$relative)}
    }
}
Archive 'FlydigiTriggerBridge-0.1.2-source.zip' $source
$portable=@()
foreach($name in @('FlydigiTriggerBridge.exe','FlydigiTriggerBridge.exe.sha256')){$portable+=@{path=(Join-Path $root ('dist\'+$name));entry=$name}}
foreach($name in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md')){$portable+=@{path=(Join-Path $root $name);entry=$name}}
foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'docs') -File){$portable+=@{path=$file.FullName;entry=('docs/'+$file.Name)}}
Archive 'FlydigiTriggerBridge-0.1.2-windows-x64.zip' $portable
Write-Host 'Source and portable packages exclude research evidence, private profiles and signing keys.'
