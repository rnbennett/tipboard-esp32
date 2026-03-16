@echo off
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.3
set IDF_TOOLS_PATH=C:\Espressif
set PATH=C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;%PATH%
cd /d C:\Users\rnben\OneDrive\Documents\Development\tipboard
C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Espressif\tools\ninja\1.12.1\ninja.exe -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 -DPYTHON=C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe -DSDKCONFIG=C:\Users\rnben\OneDrive\Documents\Development\tipboard\sdkconfig -B build -S .
if %errorlevel% neq 0 exit /b %errorlevel%
C:\Espressif\tools\ninja\1.12.1\ninja.exe -C build
if %errorlevel% neq 0 exit /b %errorlevel%
echo BUILD_SUCCESS
