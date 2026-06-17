$ErrorActionPreference = "Stop"

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$LinuxUser = "hcj456830"
$LinuxHost = "192.168.215.128"
$LinuxProjectDir = "/home/hcj456830/sign-language-learning-system"
$SshKey = Join-Path $env:USERPROFILE ".ssh\codex_ubuntu_vm_ed25519"
$ArchivePath = Join-Path $env:TEMP "sign-language-learning-system-sync.tar"

Push-Location $ProjectDir
try {
    if (Test-Path -LiteralPath $ArchivePath) {
        Remove-Item -LiteralPath $ArchivePath -Force
    }

    tar -cf $ArchivePath `
        CMakeLists.txt `
        README.md `
        run-sign-language-system.ps1 `
        src `
        tests `
        scripts

    scp -i $SshKey $ArchivePath "${LinuxUser}@${LinuxHost}:/tmp/sign-language-learning-system-sync.tar"

    $remoteCommand = @'
set -e
mkdir -p "__LINUX_PROJECT_DIR__"
tar -xf /tmp/sign-language-learning-system-sync.tar -C "__LINUX_PROJECT_DIR__"
cat > "$HOME/launch-sign-language.sh" <<'SH'
#!/bin/sh
cd "$HOME/sign-language-learning-system" || exit 1
export DISPLAY=:0
export XAUTHORITY="$HOME/.Xauthority"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$(id -u)/bus}"
pkill -f '^./build-linux/SignLanguageLearningSystem$' 2>/dev/null || true
./build-linux/SignLanguageLearningSystem >/tmp/sign-language-app.log 2>&1 &
echo $! >/tmp/sign-language-app.pid
SH
chmod +x "$HOME/launch-sign-language.sh"
DESKTOP_DIR="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
if [ -z "$DESKTOP_DIR" ]; then
    DESKTOP_DIR="$HOME/Desktop"
fi
mkdir -p "$DESKTOP_DIR"
cat > "$DESKTOP_DIR/SignLanguageLearningSystem.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=手语动作学习系统
Comment=启动手语动作学习系统
Exec=/home/hcj456830/launch-sign-language.sh
Path=/home/hcj456830/sign-language-learning-system
Terminal=false
Categories=Education;
DESKTOP
chmod +x "$DESKTOP_DIR/SignLanguageLearningSystem.desktop"
cd "__LINUX_PROJECT_DIR__/build-linux"
cmake --build . -- -j2
./AppLogoTests
./LoginSubmitGuardTests
./GestureLibraryCacheTests
./GestureDisplayTests
./GestureScorerTests
./UserManagerTests
'@
    $remoteCommand = $remoteCommand.Replace("__LINUX_PROJECT_DIR__", $LinuxProjectDir)

    ssh -i $SshKey "${LinuxUser}@${LinuxHost}" $remoteCommand
}
finally {
    Pop-Location
}
