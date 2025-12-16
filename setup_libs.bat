@echo off
echo ===================================================
echo   DANG CHUAN BI MOI TRUONG (VCPKG BOOTSTRAP)
echo ===================================================

:: 1. Kiểm tra xem đã có folder vcpkg chưa, nếu chưa thì tải về
if not exist "vcpkg" (
    echo [1/2] Dang tai vcpkg tu GitHub...
    git clone https://github.com/microsoft/vcpkg.git
) else (
    echo [1/2] Vcpkg da ton tai. Bo qua buoc tai.
)

:: 2. Chạy file bootstrap để tạo ra file vcpkg.exe
if not exist "vcpkg\vcpkg.exe" (
    echo [2/2] Dang khoi tao vcpkg (Bootstrap)...
    cd vcpkg
    call bootstrap-vcpkg.bat
    cd ..
) else (
    echo [2/2] Vcpkg da san sang.
)

echo.
echo ===================================================
echo   CAI DAT HOAN TAT!
echo ===================================================
echo.
echo Buoc tiep theo:
echo 1. Mo file .sln bang Visual Studio.
echo 2. Bam Build (F7) hoac Run (F5).
echo 3. Visual Studio se TU DONG doc file vcpkg.json va cai dat thu vien.
echo    (Lan dau build se mat thoi gian, hay kien nhan).
echo.
pause