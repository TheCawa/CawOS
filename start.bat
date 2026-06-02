@echo off
set IMAGE=CawOS.img

if not exist %IMAGE% (
    echo [ERROR] %IMAGE% not found! Run build.bat first.
    pause
    exit /b
)

echo Starting CawOS...

qemu-system-i386 ^
    -drive format=raw,file=%IMAGE%,if=ide,index=0,media=disk ^
    -m 256 ^
    -vga std ^
    -serial stdio ^
    -audiodev driver=dsound,id=snd0 ^
    -device ac97,audiodev=snd0

if %errorlevel% neq 0 (
    echo [ERROR] QEMU failed to start. Check if it is installed and in your PATH.
    pause
)