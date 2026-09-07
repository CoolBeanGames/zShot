param (
    [int]$LatestBuild
)

$DownloadUrl = "https://github.com/CoolBeanGames/zShot/releases/download/zShot_$LatestBuild/zShot.exe"
$TempExe = Join-Path $PSScriptRoot "zShot_new.exe"
$TargetExe = Join-Path $PSScriptRoot "zShot.exe"

# Wait a moment for zShot to close
Start-Sleep -Seconds 2

# Download the new file
Invoke-WebRequest -Uri $DownloadUrl -OutFile $TempExe

if (Test-Path $TempExe) {
    # Replace the file
    Move-Item -Path $TempExe -Destination $TargetExe -Force
    # Relaunch
    Start-Process -FilePath $TargetExe -WorkingDirectory $PSScriptRoot
}
