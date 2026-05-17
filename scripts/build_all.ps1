# Build all board environments in parallel and report results.
# Mirrors scripts/build_all.sh — keep the env list in sync.
#
# Usage:
#   $env:PLATFORMIO_BUILD_SRC_FLAGS = '-DFIRMWARE_VERSION=' + "'`"1.8.1`"'"
#   .\scripts\build_all.ps1            # 4 parallel jobs
#   .\scripts\build_all.ps1 -Jobs 6    # override parallelism

param(
    [int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'

$Envs = @(
    'm5stickcplus_11',
    'm5stickcplus_2',
    't_lora_pager',
    'm5_cardputer',
    'm5_cardputer_adv',
    't_display',
    't_display_s3',
    'diy_smoochie',
    't_embed_cc1101',
    'm5_cores3',
    'm5sticks3',
    'cyd_2432w328r'
)

$LogDir = Join-Path $env:TEMP ("pio_build_" + [System.IO.Path]::GetRandomFileName().Substring(0, 8))
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null

Write-Host "Building $($Envs.Count) environments with $Jobs parallel jobs..."
Write-Host "Logs: $LogDir"
Write-Host ""

# Start-Job spawns a fresh pwsh that defaults to ~/Documents — capture the
# project root and env var so each job inherits them explicitly.
$ProjectRoot = (Get-Location).Path
$SrcFlags    = $env:PLATFORMIO_BUILD_SRC_FLAGS

$JobsList = @()
foreach ($e in $Envs) {
    # Throttle: wait until running count is below the cap.
    while (($JobsList | Where-Object { $_.State -eq 'Running' }).Count -ge $Jobs) {
        Start-Sleep -Milliseconds 500
    }

    $log = Join-Path $LogDir "$e.log"
    Write-Host "  Started: $e"
    $job = Start-Job -Name $e -ScriptBlock {
        param($EnvName, $LogPath, $Flags, $Root)
        Set-Location $Root
        if ($Flags) { $env:PLATFORMIO_BUILD_SRC_FLAGS = $Flags }
        & pio run -e $EnvName *>&1 | Out-File -FilePath $LogPath -Encoding utf8
        exit $LASTEXITCODE
    } -ArgumentList $e, $log, $SrcFlags, $ProjectRoot
    $JobsList += $job
}

# Wait for everything to finish.
$JobsList | Wait-Job | Out-Null

Write-Host ""
Write-Host "======================================"
Write-Host "RESULTS:"

$Passed = @()
$Failed = @()
foreach ($job in $JobsList) {
    Receive-Job $job *>$null
    if ($job.State -eq 'Completed' -and $job.ChildJobs[0].JobStateInfo.Reason -eq $null) {
        # Start-Job doesn't propagate native exit codes — re-check the log tail
        # for the PlatformIO success banner instead.
        $log = Join-Path $LogDir "$($job.Name).log"
        $tail = if (Test-Path $log) { Get-Content $log -Tail 5 -ErrorAction SilentlyContinue } else { @() }
        if ($tail -match 'SUCCESS') {
            $Passed += $job.Name
        } else {
            $Failed += $job.Name
        }
    } else {
        $Failed += $job.Name
    }
    Remove-Job $job -Force
}

if ($Passed.Count -gt 0) { Write-Host ("  PASSED: " + ($Passed -join ' ')) }

if ($Failed.Count -gt 0) {
    Write-Host ("  FAILED: " + ($Failed -join ' '))
    Write-Host ""
    foreach ($f in $Failed) {
        $log = Join-Path $LogDir "$f.log"
        if (Test-Path $log) {
            Write-Host "--- $f (last 20 lines) ---"
            Get-Content $log -Tail 20
            Write-Host ""
        }
    }
    Write-Host "Full logs in: $LogDir"
    exit 1
}

Write-Host "  All builds passed!"
Remove-Item -Recurse -Force $LogDir -ErrorAction SilentlyContinue
exit 0