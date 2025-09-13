$ErrorActionPreference = 'Stop'

$dest = "src.zip"
# Zbierz pliki z obu katalogów (rekurencyjnie) z podanymi rozszerzeniami
$files = Get-ChildItem -Path ".\src", ".\simulations" -Recurse -File `
         -Include *.cc, *.h, *.ned, *.ini

if (-not $files) {
    Write-Host "Brak plików do spakowania." -ForegroundColor Yellow
    exit 0
}

# Usuń poprzedni zip jeśli istnieje
if (Test-Path $dest) { Remove-Item $dest }

# Zamień na ścieżki względne, żeby w ZIP była poprawna struktura
$rel = $files | ForEach-Object { Resolve-Path -Relative $_.FullName }

Compress-Archive -Path $rel -DestinationPath $dest
Write-Host "Gotowe: $dest" -ForegroundColor Green