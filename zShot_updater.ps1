$ManifestUrl = "https://github.com/CoolBeanGames/zShot/releases/latest/download/update_manifest.xml"
$ZipUrl = "https://github.com/CoolBeanGames/zShot/releases/latest/download/zShot.zip"

$LocalManifestPath = Join-Path $PSScriptRoot "update_manifest.xml"
$TempManifestPath = Join-Path $PSScriptRoot "update_manifest_new.xml"

# Download latest manifest
try {
    Invoke-WebRequest -Uri $ManifestUrl -OutFile $TempManifestPath -UseBasicParsing
} catch {
    exit 0
}

if (-not (Test-Path $TempManifestPath)) { exit 0 }

[xml]$NewManifest = Get-Content $TempManifestPath
$NeedsUpdate = $false

if (Test-Path $LocalManifestPath) {
    [xml]$OldManifest = Get-Content $LocalManifestPath

    # Check all items for version changes or missing files
    foreach ($newItem in $NewManifest.manifest.item) {
        $oldItem = $OldManifest.manifest.item | Where-Object { $_.name -eq $newItem.name }
        if (-not $oldItem -or $oldItem.version -ne $newItem.version -or -not (Test-Path (Join-Path $PSScriptRoot $newItem.name))) {
            $NeedsUpdate = $true
            break
        }
    }

    # Check for items removed from manifest
    if (-not $NeedsUpdate) {
        foreach ($oldItem in $OldManifest.manifest.item) {
            $newItem = $NewManifest.manifest.item | Where-Object { $_.name -eq $oldItem.name }
            if (-not $newItem) {
                $NeedsUpdate = $true
                break
            }
        }
    }
} else {
    # No local manifest — update everything
    $NeedsUpdate = $true
}

if (-not $NeedsUpdate) {
    Remove-Item $TempManifestPath -Force -ErrorAction SilentlyContinue
    exit 0
}

# Close zShot
Get-Process -Name "zShot" -ErrorAction SilentlyContinue | Stop-Process -Force

# Wait for it to fully exit
$timeout = 10
while ((Get-Process -Name "zShot" -ErrorAction SilentlyContinue) -and $timeout -gt 0) {
    Start-Sleep -Milliseconds 500
    $timeout--
}

$TempZip = Join-Path $PSScriptRoot "zShot_new.zip"
$TempDir = Join-Path $PSScriptRoot "zShot_update_temp"

Invoke-WebRequest -Uri $ZipUrl -OutFile $TempZip -UseBasicParsing

if (-not (Test-Path $TempZip)) { exit 1 }

if (Test-Path $TempDir) { Remove-Item -Path $TempDir -Recurse -Force }
Expand-Archive -Path $TempZip -DestinationPath $TempDir -Force

# Delete files removed from the manifest
if (Test-Path $LocalManifestPath) {
    [xml]$OldManifest = Get-Content $LocalManifestPath
    foreach ($oldItem in $OldManifest.manifest.item) {
        $newItem = $NewManifest.manifest.item | Where-Object { $_.name -eq $oldItem.name }
        if (-not $newItem) {
            $itemPath = Join-Path $PSScriptRoot $oldItem.name
            if (Test-Path $itemPath) { Remove-Item $itemPath -Recurse -Force }
        }
    }
}

# Copy all updated files (including the updater itself — it's not currently running from $PSScriptRoot lock)
Get-ChildItem -Path $TempDir | ForEach-Object {
    $dest = Join-Path $PSScriptRoot $_.Name
    if ($_.PSIsContainer) {
        Copy-Item -Path $_.FullName -Destination $dest -Recurse -Force
    } else {
        # For the updater itself, write to a temp name and schedule rename via cmd
        if ($_.Name -eq "zShot_updater.ps1") {
            Copy-Item -Path $_.FullName -Destination "$dest.new" -Force
            # Rename after this script exits
            Start-Process "cmd.exe" -ArgumentList "/c timeout /t 1 /nobreak >nul && move /y `"$dest.new`" `"$dest`"" -WindowStyle Hidden
        } else {
            Copy-Item -Path $_.FullName -Destination $dest -Force
        }
    }
}

Remove-Item -Path $TempDir -Recurse -Force
Remove-Item -Path $TempZip -Force

# Replace manifest
Move-Item -Path $TempManifestPath -Destination $LocalManifestPath -Force

# Relaunch zShot
Start-Process -FilePath (Join-Path $PSScriptRoot "zShot.exe") -WorkingDirectory $PSScriptRoot
exit 0
