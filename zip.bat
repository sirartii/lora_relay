@echo off
REM uruchom pack.ps1 w tym samym katalogu co .bat
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0zip.ps1"
pause
