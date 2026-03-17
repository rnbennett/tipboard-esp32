$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.3"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:ESP_IDF_VERSION = "5.5"
$env:PATH = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;$env:PATH"
$PYTHON = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

# Override board selection regardless of .env
$env:TIPBOARD_BOARD = "cyd"
Set-Location "C:\Users\rnben\OneDrive\Documents\Development\tipboard"

Write-Host "=== CMAKE CONFIGURE (CYD - ESP32) ==="
& cmake -G Ninja "-DCMAKE_MAKE_PROGRAM=C:\Espressif\tools\ninja\1.12.1\ninja.exe" -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 "-DPYTHON=$PYTHON" "-DSDKCONFIG=C:\Users\rnben\OneDrive\Documents\Development\tipboard\sdkconfig.cyd" "-DSDKCONFIG_DEFAULTS=C:\Users\rnben\OneDrive\Documents\Development\tipboard\sdkconfig.defaults.cyd" "-DIDF_TARGET=esp32" -B build_cyd -S .
if ($LASTEXITCODE -ne 0) { Write-Host "CMAKE_FAILED"; exit 1 }

Write-Host "=== NINJA BUILD ==="
& ninja -C build_cyd
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD_FAILED"; exit 1 }

Write-Host "=== FLASH (COM4) ==="
& $PYTHON "$env:IDF_PATH\components\esptool_py\esptool\esptool.py" -p COM4 -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 build_cyd\bootloader\bootloader.bin 0x20000 build_cyd\tipboard.bin 0x8000 build_cyd\partition_table\partition-table.bin 0xf000 build_cyd\ota_data_initial.bin
if ($LASTEXITCODE -ne 0) { Write-Host "FLASH_FAILED"; exit 1 }

Write-Host "=== ALL DONE ==="
