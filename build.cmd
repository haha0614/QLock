@echo off
setlocal
set FW=C:\Windows\Microsoft.NET\Framework64\v4.0.30319
set ROOT=%~dp0
if not exist "%ROOT%bin" mkdir "%ROOT%bin"
"%FW%\csc.exe" /nologo /target:winexe /main:QLock.Program /optimize+ /out:"%ROOT%bin\QLock.exe" /win32manifest:"%ROOT%src\app.manifest" /win32icon:"%ROOT%src\QLock.ico" ^
 /r:"%FW%\WPF\PresentationFramework.dll" /r:"%FW%\WPF\PresentationCore.dll" /r:"%FW%\WPF\WindowsBase.dll" ^
 /r:"%FW%\System.Xaml.dll" /r:"%FW%\System.Windows.Forms.dll" /r:"%FW%\System.Drawing.dll" /r:"%FW%\System.Runtime.Serialization.dll" /r:"%FW%\System.Security.dll" ^
 "%ROOT%src\*.cs"
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK -^> %ROOT%bin\QLock.exe
