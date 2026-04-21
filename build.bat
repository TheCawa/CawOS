@echo off
setlocal enabledelayedexpansion
color 07

echo =================================
echo          Building CawOS
echo =================================

if not exist build mkdir build
if not exist build\apps mkdir build\apps

:: 1. ASM
echo [ASM] Compiling low-level objects...
nasm src/boot/kernel_entry.asm -f elf32 -o build/kernel_entry.o || goto error
nasm src/cpu/interrupt.asm -f elf32 -o build/interrupt.o || goto error

:: 2. C
echo [C] Compiling kernel modules...
for /r src %%f in (*.c) do (
    echo %%f | findstr /I "\\bin\\" >nul
    if !errorlevel! neq 0 (
        echo   Compiling %%~nxf...
        i686-elf-gcc -ffreestanding -fno-pie -fno-stack-protector -m32 -Iinclude -Wall -O2 -c "%%f" -o "build/%%~nf.o" || goto error
    )
)

:: 3. Сбор списка объектников
set "CORE_OBJS=build/kernel_entry.o build/interrupt.o"
for %%i in (build\*.o) do (
    if not "%%~nxi"=="kernel_entry.o" (
        if not "%%~nxi"=="interrupt.o" (
            set "CORE_OBJS=!CORE_OBJS! build\%%~nxi"
        )
    )
)

:: 4. Линковка
echo [LD] Linking kernel.elf...
i686-elf-ld -m elf_i386 -T scripts/linker.ld -nostdlib !CORE_OBJS! -o build/kernel.elf || goto error

:: 5. Бинарник
i686-elf-objcopy -O binary build/kernel.elf build/kernel.bin || goto error

:: 6. Приложения
echo [BIN] Compiling user programs...
if not exist build\apps mkdir build\apps
for %%a in (src/bin/*.c) do (
    echo   App: %%~nxa...
    i686-elf-gcc -ffreestanding -fno-pie -fno-stack-protector -m32 -Iinclude -c "%%a" -o "build/apps/%%~na.o" || goto error
    i686-elf-ld -m elf_i386 -Ttext 0x50000 --oformat binary "build/apps/%%~na.o" -o "build/apps/%%~na.bin" || goto error
)

:: 7. Загрузчик
for %%I in (build\kernel.bin) do set "K_SIZE=%%~zI"
set /a K_SECTORS=(%K_SIZE% / 512) + 1
nasm src/boot/boot.asm -f bin -dKERNEL_SECTORS=%K_SECTORS% -o build/boot.bin || goto error

:: 8. Склейка
echo [IMAGE] Creating final image...
copy /b build\boot.bin + build\kernel.bin os-image.bin > nul

if exist scripts/pack_fs.py (
    python scripts/pack_fs.py os-image.bin build/apps/
)

python scripts/pad_image.py os-image.bin 524288


echo.
color 0A
echo [SUCCESS] CawOS is ready!
pause
exit /b

:error
echo.
color 0C
echo [ERROR] Build failed! Check the output above.
pause
exit /b 1