# Infrastructure health check for Orbbec USB devices (Windows). Exits 0 if ready, 1 if not.

$ErrorActionPreference = 'Stop'

function Write-HealthFail {
    Write-Host '[HEALTH FAIL] Device infrastructure not ready (infrastructure failure, not a test failure).'
}

# Get-PnpDevice ships with Windows 10/11 (PnpDevice module). If it is missing,
# the runner image is not suitable for hardware tests.
$cmd = Get-Command -Name 'Get-PnpDevice' -ErrorAction SilentlyContinue
if ($null -eq $cmd) {
    Write-HealthFail
    exit 1
}

# Orbbec USB vendor ID is 2bc5; it appears in the device InstanceId as VID_2BC5.
$devices = @(Get-PnpDevice -Present -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match 'VID_2BC5' })

if ($devices.Count -eq 0) {
    Write-HealthFail
    exit 1
}

Write-Host '[HEALTH OK] Device infrastructure is ready.'
exit 0
