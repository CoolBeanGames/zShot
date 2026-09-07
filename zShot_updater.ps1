param (
    [string]$ScriptDir
)

# Fallback: use $PSScriptRoot if param not provided, then current directory
if (-not $ScriptDir) { $ScriptDir = $PSScriptRoot }
if (-not $ScriptDir) { $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $ScriptDir) { $ScriptDir = (Get-Location).Path }

$LogPath = Join-Path $ScriptDir "zShot_update.log"
function Log($msg) { "[$(Get-Date -Format 'HH:mm:ss')] $msg" | Add-Content $LogPath }

Log "Updater started. ScriptDir=$ScriptDir"

$ManifestUrl = "https://github.com/CoolBeanGames/zShot/releases/latest/download/update_manifest.xml"
$ZipUrl = "https://github.com/CoolBeanGames/zShot/releases/latest/download/zShot.zip"

$LocalManifestPath = Join-Path $ScriptDir "update_manifest.xml"
$TempManifestPath = Join-Path $ScriptDir "update_manifest_new.xml"

Log "Downloading manifest from $ManifestUrl"
try {
    Invoke-WebRequest -Uri $ManifestUrl -OutFile $TempManifestPath -UseBasicParsing
} catch {
    Log "Manifest download failed: $_"
    exit 0
}

if (-not (Test-Path $TempManifestPath)) {
    Log "Manifest file not found after download."
    exit 0
}

[xml]$NewManifest = Get-Content $TempManifestPath
$NeedsUpdate = $false

if (Test-Path $LocalManifestPath) {
    [xml]$OldManifest = Get-Content $LocalManifestPath
    Log "Local manifest found. Comparing versions..."

    foreach ($newItem in $NewManifest.manifest.item) {
        $oldItem = $OldManifest.manifest.item | Where-Object { $_.name -eq $newItem.name }
        $filePath = Join-Path $ScriptDir $newItem.name
        if (-not $oldItem -or $oldItem.version -ne $newItem.version -or -not (Test-Path $filePath)) {
            Log "Update needed for: $($newItem.name) (old=$($oldItem.version) new=$($newItem.version) exists=$(Test-Path $filePath))"
            $NeedsUpdate = $true
            break
        }
    }

    if (-not $NeedsUpdate) {
        foreach ($oldItem in $OldManifest.manifest.item) {
            $newItem = $NewManifest.manifest.item | Where-Object { $_.name -eq $oldItem.name }
            if (-not $newItem) {
                Log "Item removed from manifest: $($oldItem.name)"
                $NeedsUpdate = $true
                break
            }
        }
    }
} else {
    Log "No local manifest found — full update required."
    $NeedsUpdate = $true
}

if (-not $NeedsUpdate) {
    Log "Already up to date."
    Remove-Item $TempManifestPath -Force -ErrorAction SilentlyContinue
    exit 0
}

Log "Update required. Stopping zShot..."
Get-Process -Name "zShot" -ErrorAction SilentlyContinue | Stop-Process -Force

$timeout = 10
while ((Get-Process -Name "zShot" -ErrorAction SilentlyContinue) -and $timeout -gt 0) {
    Start-Sleep -Milliseconds 500
    $timeout--
}

$TempZip = Join-Path $ScriptDir "zShot_new.zip"
$TempDir = Join-Path $ScriptDir "zShot_update_temp"

Log "Downloading zip from $ZipUrl"
try {
    Invoke-WebRequest -Uri $ZipUrl -OutFile $TempZip -UseBasicParsing
} catch {
    Log "Zip download failed: $_"
    exit 1
}

if (-not (Test-Path $TempZip)) {
    Log "Zip not found after download."
    exit 1
}

Log "Extracting..."
if (Test-Path $TempDir) { Remove-Item -Path $TempDir -Recurse -Force }
Expand-Archive -Path $TempZip -DestinationPath $TempDir -Force

# Delete files removed from the manifest
if (Test-Path $LocalManifestPath) {
    [xml]$OldManifest = Get-Content $LocalManifestPath
    foreach ($oldItem in $OldManifest.manifest.item) {
        $newItem = $NewManifest.manifest.item | Where-Object { $_.name -eq $oldItem.name }
        if (-not $newItem) {
            $itemPath = Join-Path $ScriptDir $oldItem.name
            if (Test-Path $itemPath) {
                Log "Removing deleted item: $($oldItem.name)"
                Remove-Item $itemPath -Recurse -Force
            }
        }
    }
}

Log "Copying new files..."
Get-ChildItem -Path $TempDir | ForEach-Object {
    $dest = Join-Path $ScriptDir $_.Name
    if ($_.PSIsContainer) {
        Copy-Item -Path $_.FullName -Destination $dest -Recurse -Force
    } else {
        if ($_.Name -eq "zShot_updater.ps1") {
            Copy-Item -Path $_.FullName -Destination "$dest.new" -Force
            Start-Process "cmd.exe" -ArgumentList "/c timeout /t 1 /nobreak >nul && move /y `"$dest.new`" `"$dest`"" -WindowStyle Hidden
        } else {
            Copy-Item -Path $_.FullName -Destination $dest -Force
        }
    }
}

Remove-Item -Path $TempDir -Recurse -Force
Remove-Item -Path $TempZip -Force

Move-Item -Path $TempManifestPath -Destination $LocalManifestPath -Force
Log "Update complete. Relaunching zShot..."

Start-Process -FilePath (Join-Path $ScriptDir "zShot.exe") -WorkingDirectory $ScriptDir
exit 0
