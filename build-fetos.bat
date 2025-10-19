@echo off
setlocal enabledelayedexpansion
chcp 850 >nul

:: ===========================================
::  ⚙️  CONFIGURAÇÕES DO PROJETO
:: ===========================================
set MCU=atmega328p
set F_CPU=16000000UL
set PORT=COM5
set BAUD=115200
set PROGRAMMER=arduino

set TARGET=FetOS
set ROOT_DIR=%~dp0
set BUILD_DIR=%ROOT_DIR%build
set ELF=%BUILD_DIR%\%TARGET%.elf
set HEX=%BUILD_DIR%\%TARGET%.hex

:: ===========================================
::  🧱 COMPILAÇÃO
:: ===========================================
echo.
echo ===========================================
echo   🔧 COMPILANDO FetOS
echo ===========================================

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo [1/4] Compilando fontes...
set SRC_LIST=
for %%f in ("%ROOT_DIR%*.c") do set SRC_LIST=!SRC_LIST! "%%f"
for %%f in ("%ROOT_DIR%src\*.c") do set SRC_LIST=!SRC_LIST! "%%f"
for %%f in ("%ROOT_DIR%apps\*.c") do set SRC_LIST=!SRC_LIST! "%%f"

if "%SRC_LIST%"=="" (
    echo [ERRO] Nenhum arquivo .c encontrado!
    pause
    exit /b 1
)

for %%f in (%SRC_LIST%) do (
    echo   - %%~nxf
    avr-gcc -std=gnu99 -mmcu=%MCU% -DF_CPU=%F_CPU% ^
        -Os -flto -Wall -ffunction-sections -fdata-sections ^
        -c "%%~f" -o "%BUILD_DIR%\%%~nf.o"
    if !errorlevel! neq 0 (
        echo [ERRO] Falha ao compilar %%f
        pause
        exit /b 1
    )
)

echo.
echo [2/4] Ligando objetos...
set OBJ_LIST=
for %%f in ("%BUILD_DIR%\*.o") do set OBJ_LIST=!OBJ_LIST! "%%~f"

avr-gcc -mmcu=%MCU% -Os -flto -Wl,--gc-sections -o "%ELF%" !OBJ_LIST!

if %errorlevel% neq 0 (
    echo [ERRO] Falha no link!
    pause
    exit /b 1
)

echo.
echo [3/4] Gerando HEX...
avr-objcopy -O ihex -R .eeprom "%ELF%" "%HEX%"
if %errorlevel% neq 0 (
    echo [ERRO] Falha ao gerar HEX!
    pause
    exit /b 1
)

echo.
echo [4/4] Informações do binário:
avr-size -A "%ELF%"

echo.

echo [OK] Compilação concluída.
echo ===========================================

:: ===========================================
::  🚀 GRAVAÇÃO
:: ===========================================
echo.
echo ===========================================
echo   🚀 GRAVANDO NO ARDUINO
echo ===========================================

if not exist "%HEX%" (
    echo [ERRO] Arquivo HEX não encontrado!
    pause
    exit /b 1
)

avrdude -c %PROGRAMMER% -p %MCU% -P %PORT% -b %BAUD% -U flash:w:"%HEX%":i
if %errorlevel% neq 0 (
    echo [ERRO] Falha na gravação!
    pause
    exit /b 1
)

:: === EXECUTA FETOS HUB ===
echo.
echo ===========================================
echo   🧠  Iniciando FetOS Hub (simulador)
echo ===========================================
python "%~dp0fetos_hub.py"


echo.
echo [✅ OK] FetOS gravado com sucesso!
echo ===========================================
pause
endlocal
