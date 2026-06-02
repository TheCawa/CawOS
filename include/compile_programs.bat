@echo off
echo Compiling user programs...

echo Compiling fileview.elf...
i686-elf-gcc -nostdlib -nostartfiles -N -static -I. -Ttext=0x1000000 -o ../src/bin/elf/fileview.elf fileview.c
if errorlevel 1 goto error

echo Compiling keytest.elf...
i686-elf-gcc -nostdlib -nostartfiles -N -static -I. -Ttext=0x1000000 -o ../src/bin/elf/keytest.elf keytest.c
if errorlevel 1 goto error

echo Compiling guess_game.elf...
i686-elf-gcc -nostdlib -nostartfiles -N -static -I. -Ttext=0x1000000 -o ../src/bin/elf/guess_game.elf guess_game.c
if errorlevel 1 goto error

echo.
echo Success! Programs compiled to src/bin/
echo Run build.bat to pack them into CawOS.img
pause
exit /b 0

:error
echo.
echo Error during compilation!
pause
exit /b 1
