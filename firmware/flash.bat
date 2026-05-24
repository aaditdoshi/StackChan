@echo off
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.5_py3.11_env
call D:\Users\aadit\ESP-IDF\export.bat
cd /d %~dp0
idf.py reconfigure
idf.py -p COM3 flash monitor
pause
