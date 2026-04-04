$OutputFile = "FetOS32_SourceCode.txt"
$IgnorarArquivos = @("ArduinoJson.h")

if (Test-Path $OutputFile) { Remove-Item $OutputFile }

$Files = Get-ChildItem -File -Include *.ino, *.cpp, *.h -Recurse | Where-Object { 
    $_.DirectoryName -notmatch "\\libraries" -and 
    $_.Name -notin $IgnorarArquivos 
}

Add-Content -Path $OutputFile -Value "=========================================================="
Add-Content -Path $OutputFile -Value "  FetOS32 - CODIGO FONTE"
Add-Content -Path $OutputFile -Value "  Gerado em: $(Get-Date -Format 'dd/MM/yyyy HH:mm:ss')"
Add-Content -Path $OutputFile -Value "=========================================================="
Add-Content -Path $OutputFile -Value ""

foreach ($File in $Files) {
	    Write-Host "Processando: $($File.Name)" -ForegroundColor Cyan

    Add-Content -Path $OutputFile -Value ""
    Add-Content -Path $OutputFile -Value "// =========================================================="
    Add-Content -Path $OutputFile -Value "// ARQUIVO: $($File.Name)"
    Add-Content -Path $OutputFile -Value "// =========================================================="

    Get-Content $File.FullName | Add-Content -Path $OutputFile
}

Write-Host ""
Write-Host "Concluido! O codigo foi consolidado limpo no arquivo: $OutputFile" -ForegroundColor Green
