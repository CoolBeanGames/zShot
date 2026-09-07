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

$NeedsUpdaterUpdate = $false
$NeedsOtherUpdate = $false

if (Test-Path $LocalManifestPath) {
    [xml]$OldManifest = Get-Content $LocalManifestPath
    
    # Check updater first
    $NewUpdater = $NewManifest.manifest.item | Where-Object { $_.name -eq "zShot_updater.ps1" }
    $OldUpdater = $OldManifest.manifest.item | Where-Object { $_.name -eq "zShot_updater.ps1" }
    
    if ($NewUpdater -and ($not $OldUpdater -or $NewUpdater.version -ne $OldUpdater.version)) {
        $NeedsUpdaterUpdate = $true
    }
    
    # Check other items
    foreach ($newItem in $NewManifest.manifest.item) {
        if ($newItem.name -eq "zShot_updater.ps1") { continue }
        $oldItem = $OldManifest.manifest.item | Where-Object { $_.name -eq $newItem.name }
        if (-not $oldItem -or $oldItem.version -ne $newItem.version -or -not (Test-Path (Join-Path $PSScriptRoot $newItem.name))) {
            $NeedsOtherUpdate = $true
        }
    }
    
    # Check for deleted items
    foreach ($oldItem in $OldManifest.manifest.item) {
        $newItem = $NewManifest.manifest.item | Where-Object { $_.name -eq $oldItem.name }
        if (-not $newItem) {
            $NeedsOtherUpdate = $true
        }
    }
} else {
    # No local manifest, update everything
    $NeedsOtherUpdate = $true
}

if ($NeedsUpdaterUpdate) {
    # zShot.exe will handle replacing the updater and rerunning
    exit 99
}

if ($NeedsOtherUpdate) {
    # Updater closes zShot
    Get-Process -Name "zShot" -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1
    
    $TempZip = Join-Path $PSScriptRoot "zShot_new.zip"
    $TempDir = Join-Path $PSScriptRoot "zShot_update_temp"
    
    Invoke-WebRequest -Uri $ZipUrl -OutFile $TempZip -UseBasicParsing
    
    if (Test-Path $TempZip) {
        if (Test-Path $TempDir) { Remove-Item -Path $TempDir -Recurse -Force }
        Expand-Archive -Path $TempZip -DestinationPath $TempDir -Force
        
        # If something is not in the new manifest the files are deleted
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
        
        # Copy everything except updater
        Get-ChildItem -Path $TempDir | Where-Object { $_.Name -notmatch 'zShot_updater' } | Copy-Item -Destination $PSScriptRoot -Recurse -Force
        
        Remove-Item -Path $TempDir -Recurse -Force
        Remove-Item -Path $TempZip -Force
        
        # Replace manifest
        Move-Item -Path $TempManifestPath -Destination $LocalManifestPath -Force
        
        # Relaunch
        Start-Process -FilePath (Join-Path $PSScriptRoot "zShot.exe") -WorkingDirectory $PSScriptRoot
    }
} else {
    Remove-Item $TempManifestPath -Force
}
exit 0
