$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.3"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:PATH = "C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Program Files\Git\cmd;$env:PATH"
$PYTHON = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

Set-Location "C:\Users\rnben\OneDrive\Documents\Development\tipboard"

Write-Host "=== CMAKE CONFIGURE ==="
& cmake -G Ninja "-DCMAKE_MAKE_PROGRAM=C:\Espressif\tools\ninja\1.12.1\ninja.exe" -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 "-DPYTHON=$PYTHON" "-DSDKCONFIG=C:\Users\rnben\OneDrive\Documents\Development\tipboard\sdkconfig" -B build -S .
if ($LASTEXITCODE -ne 0) { Write-Host "CMAKE_FAILED"; exit 1 }

Write-Host "=== NINJA BUILD ==="
& ninja -C build
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD_FAILED"; exit 1 }

Write-Host "=== FLASH ==="
& $PYTHON "$env:IDF_PATH\components\esptool_py\esptool\esptool.py" -p COM8 -b 460800 --before default_reset --after hard_reset --chip esp32p4 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x2000 build\bootloader\bootloader.bin 0x20000 build\tipboard.bin 0x8000 build\partition_table\partition-table.bin 0xf000 build\ota_data_initial.bin
if ($LASTEXITCODE -ne 0) { Write-Host "FLASH_FAILED"; exit 1 }

Write-Host "=== ALL DONE ==="
