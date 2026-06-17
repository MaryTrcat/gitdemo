$ErrorActionPreference = "Stop"

$projectDir = $PSScriptRoot
$exePath = Join-Path $projectDir "build\SignLanguageLearningSystem.exe"

$env:PATH = "E:\QT\6.11.0\mingw_64\bin;E:\QT\Tools\mingw1310_64\bin;$env:PATH"

if (-not (Test-Path -LiteralPath $exePath)) {
    throw "程序不存在，请先构建项目：$exePath"
}

Start-Process -FilePath $exePath -WorkingDirectory $projectDir
