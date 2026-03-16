@echo off
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.3
set IDF_TOOLS_PATH=C:\Espressif
set PATH=C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;%PATH%
set PYTHON=C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe

cd /d C:\Users\rnben\OneDrive\Documents\Development\tipboard

echo === CMAKE CONFIGURE ===
C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Espressif\tools\ninja\1.12.1\ninja.exe -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 -DPYTHON=%PYTHON% -DSDKCONFIG=C:\Users\rnben\OneDrive\Documents\Development\tipboard\sdkconfig -B build -S .
if %errorlevel% neq 0 (echo CMAKE_FAILED & exit /b %errorlevel%)

echo === NINJA BUILD ===
C:\Espressif\tools\ninja\1.12.1\ninja.exe -C build
if %errorlevel% neq 0 (echo BUILD_FAILED & exit /b %errorlevel%)

echo === FLASH ===
%PYTHON% C:\Espressif\frameworks\esp-idf-v5.5.3\components\esptool_py\esptool\esptool.py -p COM8 -b 460800 --before default_reset --after hard_reset --chip esp32p4 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x2000 build\bootloader\bootloader.bin 0x20000 build\tipboard.bin 0x8000 build\partition_table\partition-table.bin 0xf000 build\ota_data_initial.bin
if %errorlevel% neq 0 (echo FLASH_FAILED & exit /b %errorlevel%)

echo === MONITOR ===
%PYTHON% -m esp_idf_monitor -p COM8 -b 115200 --toolchain-prefix riscv32-esp-elf- build\tipboard.elf
