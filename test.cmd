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

cl /nologo /std:c++17 /utf-8 /EHsc /MT /W3 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
   /Fo:"%ROOT%obj\\" /Fe:"%ROOT%bin\QLockTests.exe" ^
   "%ROOT%tests\cpp_smoke.cpp" "%ROOT%src\core.cpp" "%ROOT%src\theme.cpp" "%ROOT%src\font_list.cpp" ^
   "%ROOT%src\updater.cpp" ^
   /link /SUBSYSTEM:CONSOLE
if errorlevel 1 (
  echo [test] BUILD FAILED
  exit /b 1
)

"%ROOT%bin\QLockTests.exe" %*
