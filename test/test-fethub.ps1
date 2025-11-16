$ErrorActionPreference = "Stop"

function Info($msg) { Write-Host "[*] $msg" }
function Ok($msg)   { Write-Host "[OK] $msg" -ForegroundColor Green }
function Fail($msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red }

# --------------------------
# Python localizado
# --------------------------
$pythonExe = (Get-Command python).Source
Ok "Using Python: $pythonExe"

# --------------------------
# Start FetHub
# --------------------------
Info "Starting FetHub..."
$hub = Start-Process -FilePath $pythonExe `
    -ArgumentList "-m fethub" `
    -PassThru

# --------------------------
# Espera o Hub iniciar (tentando IPC)
# --------------------------
$connected = $false

for ($i = 0; $i -lt 20; $i++) {
    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $client.Connect("127.0.0.1",5678)
        $connected = $true
        break
    } catch {
        Start-Sleep -Milliseconds 300
    }
}

if (-not $connected) {
    Fail "Hub is not responding on IPC (127.0.0.1:5678)"
    Stop-Process -Id $hub.Id -Force
    exit 1
}

Ok "Hub IPC is alive"

# --------------------------
# Enviar comandos
# --------------------------
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$reader = New-Object System.IO.StreamReader($stream)
$writer.AutoFlush = $true

# teste 1: listar TVs
$writer.WriteLine("tv.list")
$response = $reader.ReadLine()
Ok "tv.list response: $response"

# teste 2: UI TEXT
$writer.WriteLine("ui.text tv:OLED_SIM 0 0 TEST_FROM_SCRIPT")
$response2 = $reader.ReadLine()
Ok "ui.text response: $response2"

# --------------------------
# Cleanup
# --------------------------
Stop-Process -Id $hub.Id -Force
Ok "Hub stopped"
