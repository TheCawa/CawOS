@echo off
set IMAGE=os-image.bin

:: 1. Проверка наличия образа
if not exist %IMAGE% (
    echo [ERROR] %IMAGE% not found! Run build.bat first.
    pause
    exit /b
)

echo Starting CawOS...
:: 2. Запуск QEMU
:: Добавил -m 256 для задания объема ОЗУ (хотя CawOS пока столько не ест)
:: Добавил -serial stdio, чтобы ты мог выводить логи из ядра в консоль
qemu-system-i386 ^
    -drive format=raw,file=%IMAGE%,if=ide,index=0,media=disk,readonly=off ^
    -audiodev driver=dsound,id=spk ^
    -machine pcspk-audiodev=spk ^
    -vga std ^
    -m 256 ^
    -serial stdio

if %errorlevel% neq 0 (
    echo [ERROR] QEMU failed to start. Check if it is installed and in your PATH.
    pause
)