$ErrorActionPreference = "Stop"

$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Desktop = [Environment]::GetFolderPath("Desktop")
$Startup = [Environment]::GetFolderPath("Startup")
$WScriptShell = New-Object -ComObject WScript.Shell

$startShortcut = $WScriptShell.CreateShortcut((Join-Path $Desktop "手语动作学习系统.lnk"))
$startShortcut.TargetPath = "$ProjectDir\build\SignLanguageLearningSystem.exe"
$startShortcut.Arguments = ""
$startShortcut.WorkingDirectory = $ProjectDir
$startShortcut.IconLocation = "$ProjectDir\build\SignLanguageLearningSystem.exe,0"
$startShortcut.Save()

$desktopSyncShortcuts = @(
    (Join-Path $Desktop "手语系统-自动同步Linux.lnk"),
    (Join-Path $Desktop "手语动作学系统.lnk")
)
foreach ($shortcut in $desktopSyncShortcuts) {
    if (Test-Path -LiteralPath $shortcut) {
        Remove-Item -LiteralPath $shortcut -Force
    }
}

$startupShortcut = $WScriptShell.CreateShortcut((Join-Path $Startup "手语系统-自动同步Linux.lnk"))
$startupShortcut.TargetPath = "wscript.exe"
$startupShortcut.Arguments = "`"$ProjectDir\scripts\watch-sync-hidden.vbs`""
$startupShortcut.WorkingDirectory = $ProjectDir
$startupShortcut.IconLocation = "$ProjectDir\build\SignLanguageLearningSystem.exe,0"
$startupShortcut.Save()

Write-Host "Created shortcuts:"
Write-Host " - $Desktop\手语动作学习系统.lnk"
Write-Host " - $Startup\手语系统-自动同步Linux.lnk"
