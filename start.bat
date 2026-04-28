@echo off
set IMAGE=os-image.bin

if not exist %IMAGE% (
    echo [ERROR] %IMAGE% not found! Run build.bat first.
    pause
    exit /b
)

echo Starting CawOS...

qemu-system-i386 ^
    -drive format=raw,file=%IMAGE%,if=ide,index=0,media=disk,readonly=off,cache=none ^
    -audiodev driver=dsound,id=spk ^
    -machine pcspk-audiodev=spk ^
    -vga std ^
    -m 256 ^
    -serial stdio

if %errorlevel% neq 0 (
    echo [ERROR] QEMU failed to start. Check if it is installed and in your PATH.
    pause
)