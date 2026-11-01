@echo off
rem SWEAX NEXT - derlenmis VST3'u kullanici VST3 klasorune kopyalar (yonetici gerektirmez).
rem Steinberg VST3 spec'i %LOCALAPPDATA%\Programs\Common\VST3 yolunu da tarar (FL, Live, Cubase, S1, REAPER).
setlocal
set SRC=%~dp0..\build\SweaxNext_artefacts\Release\VST3\SWEAX NEXT.vst3
set DST=%LOCALAPPDATA%\Programs\Common\VST3\SWEAX NEXT.vst3
if not exist "%SRC%" ( echo ONCE DERLE: Tools\derle.cmd Release SweaxNext_VST3 & exit /b 1 )
robocopy "%SRC%" "%DST%" /MIR /NFL /NDL /NJH /NJS >nul
if %errorlevel% geq 8 ( echo KOPYALAMA HATASI & exit /b 1 )
echo KURULDU: %DST%
