$ErrorActionPreference = "Stop"

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$SyncScript = Join-Path $PSScriptRoot "sync-to-linux.ps1"
$LogPath = Join-Path $ProjectDir "sync-to-linux.log"
$Global:SyncRunning = $false
$Global:PendingSync = $false

function Invoke-ProjectSync {
    if ($Global:SyncRunning) {
        $Global:PendingSync = $true
        return
    }

    do {
        $Global:PendingSync = $false
        $Global:SyncRunning = $true
        try {
            "`n[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] sync start" | Add-Content -LiteralPath $LogPath
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $SyncScript *>> $LogPath
            "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] sync ok" | Add-Content -LiteralPath $LogPath
        }
        catch {
            "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] sync failed: $($_.Exception.Message)" | Add-Content -LiteralPath $LogPath
        }
        finally {
            $Global:SyncRunning = $false
        }
    } while ($Global:PendingSync)
}

Invoke-ProjectSync

$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $ProjectDir
$watcher.IncludeSubdirectories = $true
$watcher.EnableRaisingEvents = $true
$watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, DirectoryName, LastWrite, Size'

$ignored = '\\build\\|\\build-linux\\|\\.qtcreator\\|\\.vscode\\|sync-to-linux\.log$|\.tmp$|~$'
$timer = New-Object Timers.Timer
$timer.Interval = 1800
$timer.AutoReset = $false
$timer.add_Elapsed({ Invoke-ProjectSync })

$action = {
    $path = $Event.SourceEventArgs.FullPath
    if ($path -match $ignored) {
        return
    }
    $timer.Stop()
    $timer.Start()
}

Register-ObjectEvent $watcher Changed -Action $action | Out-Null
Register-ObjectEvent $watcher Created -Action $action | Out-Null
Register-ObjectEvent $watcher Deleted -Action $action | Out-Null
Register-ObjectEvent $watcher Renamed -Action $action | Out-Null

"[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] watcher started" | Add-Content -LiteralPath $LogPath
while ($true) {
    Start-Sleep -Seconds 5
}
