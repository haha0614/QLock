@echo off
setlocal
set "ROOT=%~dp0"
where cl.exe >nul 2>nul
if errorlevel 1 (
  call "%ROOT%tools\vcvars.cmd"
  if errorlevel 1 exit /b 1
)
if not exist "%ROOT%obj" mkdir "%ROOT%obj"
if not exist "%ROOT%bin" mkdir "%ROOT%bin"

if not exist "%ROOT%src\QLock.ico" (
  echo [build] missing src\QLock.ico
  exit /b 1
)
rc /nologo /I "%ROOT%src" /fo "%ROOT%obj\qlock.res" "%ROOT%src\qlock.rc"
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
   /Fo:"%ROOT%obj\\" /Fe:"%ROOT%bin\QLock.exe" ^
   "%ROOT%src\*.cpp" "%ROOT%obj\qlock.res" ^
   /link /SUBSYSTEM:WINDOWS ^
   /MANIFEST:EMBED /MANIFESTINPUT:"%ROOT%src\app.manifest"
if errorlevel 1 (
  echo [build] FAILED
  exit /b 1
)
echo [build] OK -^> bin\QLock.exe
exit /b 0
