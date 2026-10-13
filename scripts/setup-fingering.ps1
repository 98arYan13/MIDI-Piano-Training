param([switch]$Offline, [switch]$PinnedMirror, [switch]$ConfirmRedistributionRights)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Join-Path $projectRoot 'fingering/dependencies.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$runtimeRoot = Join-Path $projectRoot '.tools/fingering'
$downloadRoot = Join-Path $runtimeRoot 'downloads'
$wheelRoot = $downloadRoot
$pythonRoot = Join-Path $runtimeRoot 'python'
$sitePackages = Join-Path $pythonRoot 'Lib/site-packages'
$modelRoot = Join-Path $runtimeRoot 'models'
New-Item -ItemType Directory -Force $downloadRoot,$wheelRoot,$modelRoot | Out-Null

function Ensure-File($path, $url, $sha256) {
    if (-not (Test-Path -LiteralPath $path)) {
        if ($Offline) { throw "Offline setup is missing cached file: $path" }
        Invoke-WebRequest -Uri $url -OutFile $path
    }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $sha256.ToLowerInvariant()) { throw "SHA-256 mismatch: $path" }
}

$pythonZip = Join-Path $downloadRoot 'python-3.11.9-embed-amd64.zip'
Ensure-File $pythonZip $manifest.runtime.archive $manifest.runtime.sha256
if (-not (Test-Path (Join-Path $pythonRoot 'python.exe'))) {
    Expand-Archive -LiteralPath $pythonZip -DestinationPath $pythonRoot
}
New-Item -ItemType Directory -Force $sitePackages | Out-Null
$pth = Join-Path $pythonRoot 'python311._pth'
$pthText = @('python311.zip', '.', 'Lib\site-packages', 'import site') -join "`r`n"
Set-Content -LiteralPath $pth -Value $pthText -Encoding ASCII

foreach ($hand in @('left','right')) {
    $item = $manifest.checkpoints.$hand
    Ensure-File (Join-Path $modelRoot $item.file) $item.url $item.sha256
}

$wheelMap = @{}
foreach ($property in $manifest.wheels.PSObject.Properties) {
    $wheel = $property.Value
    $path = Join-Path $wheelRoot $wheel.filename
    if ($property.Name -in @('torch','pip')) {
        $url = $wheel.url
        if ($property.Name -eq 'pip' -and $PinnedMirror) {
            $url = 'https://pypi.tuna.tsinghua.edu.cn/packages/f3/6e/1736e5b4ae2b778ef2f81c47d797de9f891d4d8acb047a24ca37a60294dd/pip-26.2.1-py3-none-any.whl'
        }
        Ensure-File $path $url $wheel.sha256
    }
}
$pipArchive = Join-Path $wheelRoot $manifest.wheels.pip.filename
if (-not (Test-Path (Join-Path $sitePackages 'pip/__init__.py'))) {
    & (Join-Path $pythonRoot 'python.exe') (Join-Path $projectRoot 'scripts/bootstrap-pip.py') $pipArchive $sitePackages $manifest.wheels.pip.version
    if ($LASTEXITCODE -ne 0) { throw 'Could not bootstrap pinned pip' }
}
$requirements = Join-Path $projectRoot 'fingering/requirements.lock'
if ($Offline) {
    & (Join-Path $pythonRoot 'python.exe') -m pip download --no-index --find-links $wheelRoot --only-binary=:all: --require-hashes -d $wheelRoot -r $requirements
} else {
    $pypiIndex = if ($PinnedMirror) { 'https://pypi.tuna.tsinghua.edu.cn/simple' } else { 'https://pypi.org/simple' }
    & (Join-Path $pythonRoot 'python.exe') -m pip download --index-url https://download.pytorch.org/whl/cpu --extra-index-url $pypiIndex --find-links $wheelRoot --only-binary=:all: --require-hashes -d $wheelRoot -r $requirements
}
if ($LASTEXITCODE -ne 0) { throw 'Pinned ArGNN wheel download failed' }
& (Join-Path $pythonRoot 'python.exe') -m pip install --no-index --find-links $wheelRoot --only-binary=:all: --require-hashes -r $requirements
if ($LASTEXITCODE -ne 0) { throw 'Pinned ArGNN wheel installation failed' }
if ($ConfirmRedistributionRights) {
    & "$PSScriptRoot/setup-fingering-msvc.ps1" -Offline:$Offline -ConfirmRedistributionRights
}
foreach ($dll in $manifest.msvcRuntime.files.PSObject.Properties) {
    $path = Join-Path $pythonRoot $dll.Name
    if (-not (Test-Path $path) -or (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dll.Value) {
        throw 'Missing or invalid project-local MSVC runtime. Applicable Visual Studio redistribution rights must be confirmed with -ConfirmRedistributionRights; system DLLs are not portable evidence.'
    }
}
& (Join-Path $pythonRoot 'python.exe') -c "import sys,warnings; warnings.filterwarnings('ignore',message='Failed to initialize NumPy:',category=UserWarning); import torch; assert sys.version_info[:3] == (3,11,9); assert torch.__version__ == '2.8.0+cpu'; print(sys.version); print(torch.__version__)"
if ($LASTEXITCODE -ne 0) { throw 'Installed Python/Torch runtime failed validation' }
