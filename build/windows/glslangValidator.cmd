@echo off
python -I "%~dp0..\glslang_shim.py" %*
exit /b %ERRORLEVEL%
