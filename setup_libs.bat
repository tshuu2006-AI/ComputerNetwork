@echo off
setlocal enabledelayedexpansion

REM =========================
REM URLs
REM =========================
set OPENCV_URL=https://github.com/opencv/opencv/releases/download/4.12.0/opencv-4.12.0-windows.exe
set SFML_URL=https://github.com/SFML/SFML/releases/download/2.6.1/SFML-2.6.1-windows-vc17-64-bit.zip

REM =========================
REM Thư mục hiện tại
REM =========================
set ROOT_DIR=%~dp0
set LIB_DIR=%ROOT_DIR%libraries

cd /d "%ROOT_DIR%"

REM =========================
REM Tạo thư mục libraries
REM =========================
mkdir "%LIB_DIR%" 2>nul

REM =========================
REM Download OpenCV
REM =========================
echo === Downloading OpenCV 4.12.0 ===
powershell -Command ^
 "Invoke-WebRequest '%OPENCV_URL%' -OutFile 'opencv.exe'"

REM =========================
REM Giải nén OpenCV vào libraries
REM =========================
echo === Extracting OpenCV ===
opencv.exe -o"%LIB_DIR%" -y >nul

REM =========================
REM Download SFML
REM =========================
echo === Downloading SFML 2.6.1 ===
powershell -Command ^
 "Invoke-WebRequest '%SFML_URL%' -OutFile 'sfml.zip'"

REM =========================
REM Giải nén SFML vào libraries
REM =========================
echo === Extracting SFML ===
powershell -Command ^
 "Expand-Archive 'sfml.zip' '%LIB_DIR%' -Force"

REM =========================
REM Xóa file tải về
REM =========================
echo === Cleaning downloaded files ===
del /f /q opencv.exe
del /f /q sfml.zip

echo.
echo DONE.
pause
