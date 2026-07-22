@echo off
cd /d c:\Users\29164\Desktop\Robot-UI\Robot-UI-master\plugins\counting
rd /s /q .venv 2>nul
"c:\Users\29164\Desktop\Robot-UI\Robot-UI-master\python\python.exe" -m virtualenv .venv
if %errorlevel% neq 0 exit /b 1
"c:\Users\29164\Desktop\Robot-UI\Robot-UI-master\plugins\counting\.venv\Scripts\python.exe" -m pip install "opencv-python" "numpy" "ultralytics" "Pillow"
