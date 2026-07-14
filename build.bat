@echo off
setlocal enabledelayedexpansion
color 07

echo =================================
echo         Building CawOS
echo =================================

if not exist build mkdir build
if not exist build\recovery mkdir build\recovery
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
        echo %%f | findstr /I "\\recovery\\" >nul
        if !errorlevel! neq 0 (
            echo     Compiling %%~nxf...
            set "file_dir=%%~dpf"
            set "rel_dir=!file_dir:%cd%\src\=!"
            set "obj_dir=build\!rel_dir!"
            if not exist "!obj_dir!" mkdir "!obj_dir!"
            i686-elf-gcc -march=i486 -ffreestanding -fno-pie -fno-stack-protector -m32 -Iinclude -Wall -O2 -Wno-array-bounds -c "%%f" -o "!obj_dir!%%~nf.o" || goto error
        )
    )
)

:: 2.5 Компиляция RECOVERY
echo [C] Compiling Recovery module...
if not exist src\recovery\recovery.c (
    echo [ERROR] src/recovery/recovery.c not found!
    goto error
)
i686-elf-gcc -march=i486 -ffreestanding -fno-pie -fno-stack-protector -m32 -Iinclude -Wall -O2 -Wno-array-bounds -c "src/recovery/recovery.c" -o "build/recovery/recovery.o" || goto error
set "FONT_OBJ="
for /r build %%g in (font.o) do (
    if exist "%%g" set "FONT_OBJ=%%g"
)
if "!FONT_OBJ!"=="" set "FONT_OBJ=build\font.o"
echo Using font object: !FONT_OBJ!
i686-elf-ld -m elf_i386 -T scripts/linker.ld -nostdlib build/recovery/recovery.o "!FONT_OBJ!" -o build/recovery/recovery.elf || goto error
i686-elf-objcopy -O binary build/recovery/recovery.elf build/recovery.bin || goto error

:: 3. Сбор списка объектников ядра
set "CORE_OBJS=build/kernel_entry.o build/interrupt.o"
for /r build %%i in (*.o) do (
    set "obj_name=%%~nxi"
    if not "!obj_name!"=="kernel_entry.o" (
        if not "!obj_name!"=="interrupt.o" (
            if not "!obj_name!"=="recovery.o" (
                set "CORE_OBJS=!CORE_OBJS! %%i"
            )
        )
    )
)

:: 4. Линковка ядра
echo [LD] Linking kernel.elf...
i686-elf-ld -m elf_i386 -T scripts/linker.ld -nostdlib !CORE_OBJS! -o build/kernel.elf || goto error

:: 5. Создаем бинарник ядра
i686-elf-objcopy -O binary build/kernel.elf build/kernel.bin || goto error

:: 6. Приложения и Ресурсы
echo [BIN] Copying user programs...
if not exist build\apps mkdir build\apps

echo   Copying resources...
xcopy /Y /Q /I /E "src\bin" "build\apps\" > nul

:: 7. Вычисляем размеры ядра и восстановления для загрузчика
for %%I in (build\kernel.bin) do set K_SIZE=%%~zI
set /a K_SECTORS=(%K_SIZE% + 511) / 512
for %%I in (build\recovery.bin) do set R_SIZE=%%~zI
set /a R_SECTORS=(%R_SIZE% + 511) / 512
set /a R_LBA=5 + %K_SECTORS% + 2
echo Kernel Size: %K_SIZE% bytes (%K_SECTORS% sectors)
echo Recovery Size: %R_SIZE% bytes (%R_SECTORS% sectors) LBA: %R_LBA%

:: 8. Компилируем загрузчик
echo [ASM] Compiling bootloader...
nasm src/boot/boot.asm -f bin -o build/boot.bin || goto error
nasm src/boot/stage2.asm -f bin -dKERNEL_SECTORS=%K_SECTORS% -dRECOVERY_SECTORS=%R_SECTORS% -dRECOVERY_LBA=%R_LBA% -o build/stage2.bin || goto error

:: 9. Создание образа
echo [IMAGE] Creating CawOS.img...
python scripts/build_img.py build/boot.bin build/stage2.bin build/kernel.bin build/recovery.bin build/apps CawOS.img

if errorlevel 1 goto error

echo.
color 0A
echo [SUCCESS] CawOS.img is ready!
pause
exit /b 0

:error
echo.
color 0C
echo [ERROR] Build failed!
pause
exit /b 1