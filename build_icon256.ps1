Add-Type -AssemblyName System.Drawing;
$img = [System.Drawing.Bitmap]::FromFile("icon.png");
$resized = New-Object System.Drawing.Bitmap($img, 256, 256);
$ms = New-Object System.IO.MemoryStream;
$resized.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png);
$pngBytes = $ms.ToArray();
$fs = [System.IO.File]::Create("icon.ico");
$bw = New-Object System.IO.BinaryWriter($fs);
$bw.Write([uint16]0); # Reserved
$bw.Write([uint16]1); # Type: ICO
$bw.Write([uint16]1); # Count: 1
$bw.Write([byte]0);   # Width (0 means 256)
$bw.Write([byte]0);   # Height (0 means 256)
$bw.Write([byte]0);   # Colors
$bw.Write([byte]0);   # Reserved
$bw.Write([uint16]1); # Color planes
$bw.Write([uint16]32); # Bits per pixel
$bw.Write([uint32]$pngBytes.Length); # Size of image data
$bw.Write([uint32]22); # Offset to image data
$bw.Write($pngBytes, 0, $pngBytes.Length);
$bw.Close();
$fs.Close();
$ms.Close();
