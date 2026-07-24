#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Developer / test helper for RXDK-VS20XX.

.DESCRIPTION
  One entry point for the fiddly iterate loop: rebuild + republish the net8 engine and
  debug adapter into %ProgramData%\RXDK\engine (where an installed VSIX and the sample
  .vcxproj files resolve them), build the sample solution, deploy a title to the kit,
  smoke-test the DAP adapter, build the VSIX, and (re)generate a debug launch config.

.EXAMPLE
  ./scripts/dev.ps1 status              # show kit IP, tool/SDK/Zig status, engine files
  ./scripts/dev.ps1 publish             # rebuild Rxdk.Cli + Rxdk.Dap -> ProgramData\RXDK\engine
  ./scripts/dev.ps1 samples             # msbuild the sample .vcxproj solution
  ./scripts/dev.ps1 deploy -Sample Game # build+deploy a sample to the kit
  ./scripts/dev.ps1 smoke               # DAP initialize/disconnect handshake test on the adapter
  ./scripts/dev.ps1 launch-json -Sample Game   # (re)write rxdk-debug.launch.json for that sample
  ./scripts/dev.ps1 vsix                # msbuild the RxdkVs.Package VSIX
  ./scripts/dev.ps1 all                 # publish + samples (the usual after a code change)
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('status', 'publish', 'samples', 'deploy', 'run', 'reboot', 'smoke', 'vsix', 'launch-json', 'all', 'help')]
    [string]$Command = 'status',

    [ValidateSet('Game', 'Empty', 'Lib', 'Dxt')]
    [string]$Sample = 'Game',

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

# ---- paths ----
$Repo       = Split-Path -Parent $PSScriptRoot          # scripts/ sits under the repo root
$EngineDir  = Join-Path $env:ProgramData 'RXDK\engine'
$Cli        = Join-Path $EngineDir 'Rxdk.Cli.exe'
$Dap        = Join-Path $EngineDir 'Rxdk.Dap.exe'
$SamplesSln = Join-Path $Repo 'samples\RXDK-Samples.sln'
$VsixProj   = Join-Path $Repo 'RxdkVs.Package\RxdkVs.Package.csproj'

function Info($m)  { Write-Host "==> $m" -ForegroundColor Cyan }
function Ok($m)    { Write-Host "OK  $m" -ForegroundColor Green }
function Warn($m)  { Write-Host "!!  $m" -ForegroundColor Yellow }

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found ($vswhere) - is Visual Studio installed?" }
    $msb = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    if (-not $msb) { throw "MSBuild not found via vswhere." }
    return $msb
}

function Get-SampleDir  { Join-Path $Repo "samples\$Sample" }
function Get-SampleName {
    $manifest = Join-Path (Get-SampleDir) 'rxdk.project.json'
    if (-not (Test-Path $manifest)) { throw "No rxdk.project.json in $(Get-SampleDir)" }
    (Get-Content $manifest -Raw | ConvertFrom-Json).name
}

# ---- commands ----

function Invoke-Publish {
    Info "Publishing Rxdk.Cli + Rxdk.Dap (net8, framework-dependent) -> $EngineDir"
    # A running VS / debug session can lock the exes; warn rather than fail cryptically.
    foreach ($p in 'Rxdk.Cli', 'Rxdk.Dap') {
        if (Get-Process $p -ErrorAction SilentlyContinue) {
            Warn "$p is running - close it (or the debug session using it) if publish fails with a file lock."
        }
    }
    foreach ($proj in 'Rxdk.Cli', 'Rxdk.Dap') {
        $csproj = Join-Path $Repo "$proj\$proj.csproj"
        dotnet publish $csproj -c Release -o $EngineDir --no-self-contained -v q
        if ($LASTEXITCODE -ne 0) { throw "publish failed for $proj" }
    }
    Ok "Engine published. Rxdk.Cli.exe / Rxdk.Dap.exe are current."
}

function Invoke-Samples {
    Info "Building sample solution ($Config|Win32)"
    $msb = Get-MSBuild
    & $msb -nologo -v:m -restore "-p:Configuration=$Config;Platform=Win32" $SamplesSln
    if ($LASTEXITCODE -ne 0) { throw "sample build failed" }
    Ok "Samples built."
}

function Invoke-Deploy {
    Info "Building+deploying '$Sample' to the kit"
    & $Cli build --project-root (Get-SampleDir) --optimize $Config
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
    & $Cli deploy --project-root (Get-SampleDir)
    if ($LASTEXITCODE -ne 0) { throw "deploy failed" }
    Ok "Deployed '$Sample'."
}

function Invoke-Run    { Info "Launching '$Sample' on the kit"; & $Cli run --project-root (Get-SampleDir) }
function Invoke-Reboot { Info "Warm-rebooting the kit"; & $Cli reboot }

function Invoke-Smoke {
    Info "DAP handshake smoke test against $Dap"
    if (-not (Test-Path $Dap)) { throw "Rxdk.Dap.exe not found - run: ./scripts/dev.ps1 publish" }

    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $Dap
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $proc = [System.Diagnostics.Process]::Start($psi)

    function Send($obj) {
        $json = $obj | ConvertTo-Json -Compress
        $bytes = [Text.Encoding]::UTF8.GetByteCount($json)
        $proc.StandardInput.Write("Content-Length: $bytes`r`n`r`n$json")
        $proc.StandardInput.Flush()
    }
    Send @{ seq = 1; type = 'request'; command = 'initialize'; arguments = @{ adapterID = 'xbox' } }
    Start-Sleep -Milliseconds 800
    Send @{ seq = 2; type = 'request'; command = 'disconnect'; arguments = @{} }
    Start-Sleep -Milliseconds 600
    $proc.StandardInput.Close()
    if (-not $proc.WaitForExit(3000)) { $proc.Kill() }
    $out = $proc.StandardOutput.ReadToEnd()

    $hasInit = $out -match '"command"\s*:\s*"initialize"'
    $hasEvt  = $out -match '"event"\s*:\s*"initialized"'
    if ($hasInit -and $hasEvt) {
        Ok "Adapter responds to DAP (initialize + initialized event). The exe is healthy."
    }
    else {
        Warn "Unexpected adapter output - the DAP handshake did not complete:"
        Write-Host $out
        throw "smoke test failed"
    }
}

function Invoke-Vsix {
    Info "Building RxdkVs.Package VSIX"
    $msb = Get-MSBuild
    & $msb -nologo -v:m -restore "-p:Configuration=Debug" $VsixProj
    if ($LASTEXITCODE -ne 0) { throw "VSIX build failed" }
    Ok "VSIX built -> RxdkVs.Package\bin\Debug\RxdkVs.Package.vsix"
}

function Write-LaunchJson {
    $name = Get-SampleName
    $dir  = Get-SampleDir
    $out  = Join-Path $dir 'out'
    $json = [ordered]@{
        '$adapter'        = $Dap
        type              = 'xbox'
        request           = 'launch'
        name              = "Debug $name"
        program           = (Join-Path $out "$name.exe")
        pdb               = (Join-Path $out "$name.pdb")
        xbePath           = "xe:\$name\$name.xbe"
        '__workspaceFolder' = $dir
        reboot            = $false
    }
    $path = Join-Path $dir 'rxdk-debug.launch.json'
    $json | ConvertTo-Json | Set-Content -Path $path -Encoding utf8
    Ok "Wrote $path"
    Write-Host ""
    Write-Host "In the VS Command Window (Ctrl+Alt+A):" -ForegroundColor Cyan
    Write-Host "  DebugAdapterHost.Logging /On /OutputWindow"
    Write-Host "  DebugAdapterHost.Launch /LaunchJson:`"$path`""
}

function Show-Status {
    Info "Kit / toolchain status"
    if (Test-Path $Cli) {
        & $Cli xbox-ip
        Write-Host ""
        & $Cli tools-status
        Write-Host ""
        & $Cli sdk-status
        Write-Host ""
        & $Cli zig-status
    }
    else {
        Warn "Rxdk.Cli.exe not found in $EngineDir - run: ./scripts/dev.ps1 publish"
    }
    Write-Host ""
    Info "Engine files in $EngineDir"
    if (Test-Path $EngineDir) {
        Get-ChildItem $EngineDir -Filter '*.exe' | Select-Object Name, @{n='Modified';e={$_.LastWriteTime}} | Format-Table -AutoSize
    } else { Warn "engine dir missing" }
}

function Show-Help { Get-Help $PSCommandPath -Detailed }

switch ($Command) {
    'status'      { Show-Status }
    'publish'     { Invoke-Publish }
    'samples'     { Invoke-Samples }
    'deploy'      { Invoke-Deploy }
    'run'         { Invoke-Run }
    'reboot'      { Invoke-Reboot }
    'smoke'       { Invoke-Smoke }
    'vsix'        { Invoke-Vsix }
    'launch-json' { Write-LaunchJson }
    'all'         { Invoke-Publish; Invoke-Samples }
    'help'        { Show-Help }
}
