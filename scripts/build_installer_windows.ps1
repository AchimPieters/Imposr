[CmdletBinding()]
param(
    [string]$BuildDir = "build-package",
    [string]$BuildType = "Release",
    [switch]$WithPlugin,
    [string]$AcrobatSdkDir,
    [string]$AcrobatPluginInstallDir = "C:\Program Files\Adobe\Acrobat DC\Acrobat\plug_ins",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

function Test-Command {
    param([Parameter(Mandatory = $true)][string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Ensure-Winget {
    if (Test-Command -Name "winget") {
        Write-Host "[OK] winget gevonden"
        return
    }

    throw "winget niet gevonden. Installeer App Installer uit de Microsoft Store en run dit script opnieuw."
}

function Ensure-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$WingetId,
        [string]$ExtraArgs = ""
    )

    if (Test-Command -Name $Command) {
        Write-Host "[OK] $Command gevonden"
        return
    }

    Write-Host "[INFO] $Command ontbreekt; installeren via winget ($WingetId)"
    $args = @("install", "--id", $WingetId, "-e", "--accept-package-agreements", "--accept-source-agreements")
    if ($ExtraArgs -ne "") {
        $args += $ExtraArgs.Split(" ")
    }

    winget @args

    if (-not (Test-Command -Name $Command)) {
        Write-Warning "$Command is nog niet in PATH. Open zo nodig een nieuwe PowerShell sessie en run opnieuw."
        throw "Installatie van $Command lijkt niet afgerond."
    }
}

Write-Host "[1/4] Controleer/verzamel vereiste tools"
Ensure-Winget
Ensure-Tool -Command "cmake" -WingetId "Kitware.CMake"
Ensure-Tool -Command "python" -WingetId "Python.Python.3.12"
Ensure-Tool -Command "cpack" -WingetId "Kitware.CMake"

if (-not (Test-Path "C:\Program Files\Microsoft Visual Studio\2022")) {
    Write-Host "[INFO] Visual Studio 2022 niet gevonden; probeer installatie via winget"
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools"
}

if ($WithPlugin) {
    if ([string]::IsNullOrWhiteSpace($AcrobatSdkDir) -and [string]::IsNullOrWhiteSpace($env:ACROBAT_SDK_DIR)) {
        throw "Gebruik je -WithPlugin? Geef dan -AcrobatSdkDir op of zet ACROBAT_SDK_DIR."
    }

    if (-not [string]::IsNullOrWhiteSpace($AcrobatSdkDir)) {
        $env:ACROBAT_SDK_DIR = $AcrobatSdkDir
    }

    if (-not (Test-Path $AcrobatPluginInstallDir)) {
        Write-Host "[INFO] Acrobat plug-in map ontbreekt; maak aan: $AcrobatPluginInstallDir"
        New-Item -ItemType Directory -Force -Path $AcrobatPluginInstallDir | Out-Null
    }

    $pluginFlag = "ON"
}
else {
    $pluginFlag = "OFF"
}

Write-Host "[2/4] Configureer packaging build"
cmake -S . -B $BuildDir -G $Generator -A $Arch `
    -DAIMP_BUILD_PLUGIN=$pluginFlag `
    -DAIMP_ACROBAT_PLUGIN_INSTALL_DIR="$AcrobatPluginInstallDir" `
    -DAIMP_BUILD_CLI=ON `
    -DAIMP_BUILD_TESTS=OFF `
    -DAIMP_ENABLE_PACKAGING=ON

Write-Host "[3/4] Bouw project"
cmake --build $BuildDir --config $BuildType

Write-Host "[4/4] Genereer installers/packages"
cpack --config "$BuildDir/CPackConfig.cmake" -C $BuildType

if ($WithPlugin) {
    $pluginFrom = Join-Path (Resolve-Path $BuildDir) "$BuildType\\AcrobatImpositionPlugin.dll"
    $pluginTo = Join-Path $AcrobatPluginInstallDir "AcrobatImpositionPlugin.dll"
    if (Test-Path $pluginFrom) {
        Copy-Item -Force $pluginFrom $pluginTo
        Write-Host "[OK] Plugin gedeployed naar: $pluginTo"
        $uninstallScript = @"
`$pluginPath = \"$pluginTo\"
if (Test-Path `$pluginPath) {
    Remove-Item -Force `$pluginPath
    Write-Host \"Plugin verwijderd: `$pluginPath\"
} else {
    Write-Host \"Plugin niet gevonden: `$pluginPath\"
}
"@
        $uninstallPath = Join-Path (Resolve-Path $BuildDir) "uninstall-plugin.ps1"
        Set-Content -Path $uninstallPath -Value $uninstallScript -Encoding UTF8
        Write-Host "[OK] Deinstaller script gemaakt: $uninstallPath"
    }
    else {
        Write-Warning "Plugin artifact niet gevonden op $pluginFrom; deploy overgeslagen."
    }
}

Write-Host "Klaar. Check artifacts in $BuildDir/"
