param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$bitmap=New-Object Drawing.Bitmap(256,256)
$graphics=[Drawing.Graphics]::FromImage($bitmap);$graphics.SmoothingMode='AntiAlias'
try{
    $graphics.Clear([Drawing.Color]::FromArgb(22,29,38))
    $shape=New-Object Drawing.Drawing2D.GraphicsPath
    $shape.AddBezier(58,87,78,64,178,64,198,87);$shape.AddBezier(198,87,216,115,228,191,201,195);$shape.AddBezier(201,195,176,200,166,159,156,148);$shape.AddLine(156,148,100,148);$shape.AddBezier(100,148,90,159,80,200,55,195);$shape.AddBezier(55,195,28,191,40,115,58,87);$shape.CloseFigure()
    $brush=New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(181,239,117));$graphics.FillPath($brush,$shape)
    $dark=New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(22,29,38));$graphics.FillRectangle($dark,74,99,10,35);$graphics.FillRectangle($dark,62,112,34,10)
    foreach($point in @(@(172,98),@(185,113),@(172,128),@(159,113))){$graphics.FillEllipse($dark,[single]$point[0],[single]$point[1],[single]10,[single]10)}
    $cyan=New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(101,216,212));$graphics.FillRectangle($cyan,65,54,42,10);$graphics.FillRectangle($cyan,149,54,42,10)
    $png=New-Object IO.MemoryStream;$bitmap.Save($png,[Drawing.Imaging.ImageFormat]::Png);$bytes=$png.ToArray()
    $file=[IO.File]::Create($OutputPath);$writer=New-Object IO.BinaryWriter($file)
    try{$writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]1);$writer.Write([byte]0);$writer.Write([byte]0);$writer.Write([byte]0);$writer.Write([byte]0);$writer.Write([uint16]1);$writer.Write([uint16]32);$writer.Write([uint32]$bytes.Length);$writer.Write([uint32]22);$writer.Write($bytes)}finally{$writer.Dispose();$png.Dispose()}
    $shape.Dispose();$brush.Dispose();$dark.Dispose();$cyan.Dispose()
}finally{$graphics.Dispose();$bitmap.Dispose()}
