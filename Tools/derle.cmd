@echo off
rem SWEAX NEXT - derleme betigi (Visual Studio 2022 ureteci; vcvars gerektirmez).
rem Kullanim: Tools\derle.cmd [Release|Debug] [hedef]   ornek: Tools\derle.cmd Release SweaxNext_VST3
rem JUCE: JUCE_DIR ortam degiskeni verilirse o kopya kullanilir, yoksa ilk configure'da 7.0.12 indirilir.
setlocal
set CM=cmake
where cmake >nul 2>nul
if errorlevel 1 (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set CM="%%i\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
set CFG=%1
if "%CFG%"=="" set CFG=Release
set HEDEF=%2
if "%HEDEF%"=="" set HEDEF=SweaxTests
cd /d "%~dp0.."
if not exist build\SweaxNext.sln (
  if defined JUCE_DIR (
    %CM% -G "Visual Studio 17 2022" -A x64 -B build "-DJUCE_DIR=%JUCE_DIR%" . || exit /b 1
  ) else (
    %CM% -G "Visual Studio 17 2022" -A x64 -B build . || exit /b 1
  )
)
%CM% --build build --config %CFG% --target %HEDEF% -- -m:8 -v:m -nologo || exit /b 1
echo DERLEME_OK %HEDEF% (%CFG%)
if "%HEDEF%"=="SweaxTests" build\SweaxTests_artefacts\%CFG%\SweaxTests.exe
