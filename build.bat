@echo off
setlocal

REM ==== CONFIG ====
REM Ruta al Open Watcom (ajústala a tu instalación)
set "WATCOM=C:\WATCOM"
set "WATCOMBIN=%WATCOM%\BINNT"
set "INCLUDE=%WATCOM%\H"
set "LIB=%WATCOM%\LIB286"

REM Ruta a DOSBox (ajústala si no lo tienes en PATH)
set "DOSBOX=H:\Programas\DOSBox-X\DOSBox-X.exe"

REM Proyecto
set "SRC=Source\main.c Source\CORE\*.c Source\GAME\*.c Source\MINI\PONG\*.c Source\MINI\INVADERS\*.c Source\MINI\BREAKOUT\*.c Source\MINI\FROG\*.c Source\MINI\TRON\*.c Source\MINI\TAPP\*.c Source\MINI\PANG\*.c Source\MINI\GORI\*.c Source\MINI\FLAPPY\*.c"
set "OUT=TIMEBUG.exe"

REM ==== BUILD ====
echo.
echo [BUILD] Compilando %SRC%  ->  %OUT%
echo.

"%WATCOMBIN%\wcl.exe" -bt=dos -ml -fe=%OUT% %SRC%
if errorlevel 1 goto :build_fail
del *.obj >nul 2>nul

REM ==== STAGE (carpeta exe) ====
echo.
echo [STAGE] Preparando carpeta .\exe\ ...
if exist "exe" rd /s /q "exe"
mkdir "exe"

copy /y "*.exe" "exe\" >nul
copy /y "*.dat" "exe\" >nul
copy /y "*.txt" "exe\" >nul

REM Copiar carpeta Sprites completa
if exist "Sprites" (
    xcopy "Sprites" "exe\Sprites\" /E /I /Y >nul
)

echo [STAGE] Copiados .exe y .dat a .\exe\
echo.

echo.
echo [OK] Build terminado: %OUT%
echo.

REM ==== RUN (DOSBox) ====
echo [RUN] Lanzando en DOSBox...
"%DOSBOX%" -defaultdir "." -c "mount c ." -c "c:" -c "echo ---- RUNNING %OUT% ----" -c "%OUT%"

echo.
echo [FIN]
goto :eof

:build_fail
echo.
echo [ERROR] El build ha petado. No se lanza DOSBox.
echo.
exit /b 1
