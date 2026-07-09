# Download official YOLO26n ONNX from Ultralytics assets (v8.4.0).
# Output: models/yolo26n.onnx (gitignored)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ModelsDir = Join-Path $Root "models"
$OutFile = Join-Path $ModelsDir "yolo26n.onnx"
$Url = "https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n.onnx"

New-Item -ItemType Directory -Force -Path $ModelsDir | Out-Null

if (Test-Path $OutFile) {
    $size = (Get-Item $OutFile).Length
    if ($size -gt 1MB) {
        Write-Host "Already present: $OutFile ($([math]::Round($size / 1MB, 1)) MB)"
        exit 0
    }
}

Write-Host "Downloading yolo26n.onnx from Ultralytics..."
Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing
Write-Host "Saved to $OutFile"
