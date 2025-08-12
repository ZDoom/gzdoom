@echo off
setlocal
cd "%~dp0"
for /f "delims=" %%I in ('where git.exe') do set GIT_PATH="%%~dpI"
if errorlevel 1 echo "Unable to find git.exe!"
if errorlevel 1 goto :eof
%GIT_PATH%\..\bin\bash.exe update-subtrees.sh %*
