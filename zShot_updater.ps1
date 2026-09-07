param (
    [int]$LatestBuild
)

$DownloadUrl = "https://github.com/CoolBeanGames/zShot/releases/download/zShot_$LatestBuild/zShot.zip"
$TempZip = Join-Path $PSScriptRoot "zShot_new.zip"
$TempDir = Join-Path $PSScriptRoot "zShot_update_temp"
$TargetExe = Join-Path $PSScriptRoot "zShot.exe"

# Wait for zShot to exit
while (Get-Process -Name "zShot" -ErrorAction SilentlyContinue) {
    Start-Sleep -Milliseconds 500
}

# Download the new file
Invoke-WebRequest -Uri $DownloadUrl -OutFile $TempZip

if (Test-Path $TempZip) {
    if (Test-Path $TempDir) { Remove-Item -Path $TempDir -Recurse -Force }
    Expand-Archive -Path $TempZip -DestinationPath $TempDir -Force
    
    # Replace the files (except the updated script)
    Get-ChildItem -Path $TempDir | Where-Object { $_.Name -notmatch 'zShot_updater' } | Copy-Item -Destination $PSScriptRoot -Recurse -Force
    
    Remove-Item -Path $TempDir -Recurse -Force
    Remove-Item -Path $TempZip -Force
    
    # Relaunch
    Start-Process -FilePath $TargetExe -WorkingDirectory $PSScriptRoot
}
