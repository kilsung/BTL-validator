@echo off
setlocal

cd /d "%~dp0"

where py >nul 2>&1
if not errorlevel 1 goto use_py

where python >nul 2>&1
if not errorlevel 1 goto use_python

echo [ERROR] Python 3 was not found. Install Python and try again.
exit /b 1

:use_py
py -3 run.py %*
exit /b %errorlevel%

:use_python
python run.py %*
exit /b %errorlevel%
