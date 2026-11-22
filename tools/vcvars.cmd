@echo off
rem 定位 Visual Studio 生成工具里的 vcvars64.bat，把 cl.exe / rc.exe 放进 PATH。
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if not exist "%VSWHERE%" goto novswhere
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSDIR=%%i"
if defined VSDIR goto have
set "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
:have
if not exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto novc
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto vcfail
exit /b 0
:novswhere
echo [vcvars] 没找到 vswhere.exe，先装 Visual Studio 生成工具 2022 的「使用 C++ 的桌面开发」
exit /b 1
:novc
echo [vcvars] 没找到 vcvars64.bat，检查生成工具是否装了「使用 C++ 的桌面开发」
exit /b 1
:vcfail
echo [vcvars] vcvars64.bat 调用失败
exit /b 1
