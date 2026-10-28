@echo off
REM Compila BLEZZ TUNE en tu PC con Windows.
REM Necesitas: Visual Studio 2022 (o Build Tools) con "Desarrollo para el escritorio con C++",
REM            CMake y Git instalados.

cmake -B build -A x64
if errorlevel 1 goto error
cmake --build build --config Release --parallel
if errorlevel 1 goto error

echo.
echo Listo. Copiando el plugin a la carpeta VST3 de Windows (pide permisos de administrador)...
xcopy /E /I /Y "build\BlezzTune_artefacts\Release\VST3\BLEZZ TUNE.vst3" "C:\Program Files\Common Files\VST3\BLEZZ TUNE.vst3"
echo.
echo Abre FL Studio ^> Options ^> Manage plugins ^> Find plugins, y busca "BLEZZ TUNE".
pause
exit /b 0

:error
echo Hubo un error al compilar. Revisa que Visual Studio C++, CMake y Git esten instalados.
pause
exit /b 1
