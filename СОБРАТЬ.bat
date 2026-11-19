@echo off
chcp 65001 >nul
title Сборка CS 1.6 - Nokia E52 Edition
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
echo Python не найден. Пробую установить его автоматически...
echo.
winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements
echo.
if %errorlevel%==0 (
  echo Python установлен. Закройте это окно и запустите СОБРАТЬ.bat ещё раз.
) else (
  echo Не получилось. Установите Python 3 с сайта python.org
  echo ^(при установке поставьте галочку "Add python.exe to PATH"^) и запустите СОБРАТЬ.bat ещё раз.
)
pause
