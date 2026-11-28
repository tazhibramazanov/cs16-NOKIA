@echo off
chcp 65001 >nul
title CS 1.6 Nokia E52 Edition - Build
cd /d "%~dp0"
py -3 --version >nul 2>nul
if %errorlevel%==0 (
  py -3 build.py %*
  goto :eof
)
python --version >nul 2>nul
if %errorlevel%==0 (
  python build.py %*
  goto :eof
)
echo Python was not found. Attempting silent install...
echo.
winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements
echo.
if %errorlevel%==0 (
  echo Python installed. Run BUILD.bat again.
) else (
  echo Install Python 3 from python.org (tick "Add python.exe to PATH"), then run BUILD.bat again.
)
pause
