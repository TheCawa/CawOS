#!/usr/bin/env python3

import os
import sys
import subprocess
import shutil
import glob
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
import multiprocessing
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"
APPS_SRC = ROOT / "src" / "bin"
APPS_BUILD = BUILD_DIR / "apps"
RECOVERY_DIR = BUILD_DIR / "recovery"

CC = "i686-elf-gcc"
LD = "i686-elf-ld"
OBJCOPY = "i686-elf-objcopy"
NASM = "nasm"

CFLAGS = [
    "-march=i486", "-ffreestanding", "-fno-pie", "-fno-stack-protector",
    "-m32", f"-I{ROOT / 'include'}", "-Wall", "-O2", "-Wno-array-bounds"
]

LDFLAGS = ["-m", "elf_i386", "-T", str(ROOT / "scripts" / "linker.ld"), "-nostdlib"]

class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    GRAY = '\033[90m'
    
    @staticmethod
    def disable():
        for attr in dir(Colors):
            if not attr.startswith('_') and attr != 'disable':
                setattr(Colors, attr, '')


def check_ansi_support():
    if os.name == 'nt':
        try:
            import ctypes
            kernel32 = ctypes.windll.kernel32
            kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
            return True
        except:
            return False
    return sys.stdout.isatty()


if not check_ansi_support():
    Colors.disable()


def run(cmd, cwd=None, check=True):
    result = subprocess.run(cmd, cwd=cwd, capture_output=True)
    if check and result.returncode != 0:
        print(f"{Colors.RED}Command failed: {' '.join(str(c) for c in cmd)}{Colors.RESET}")
        if result.stderr:
            print(result.stderr.decode())
        raise RuntimeError(f"Build failed")
    return result


def ensure_dir(path):
    path.mkdir(parents=True, exist_ok=True)


def clean_build_dir():
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    ensure_dir(BUILD_DIR)


class ProgressBar:
    def __init__(self, total, desc="Processing"):
        self.total = total
        self.current = 0
        self.desc = desc
        self.lock = threading.Lock()
        self.spinner_chars = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
        self.spinner_idx = 0
        self.last_update = time.time()
        
    def update(self, current_file=None):
        with self.lock:
            self.current += 1
            self.spinner_idx = (self.spinner_idx + 1) % len(self.spinner_chars)
            
            percent = (self.current / self.total) * 100
            bar_width = 30
            filled = int(bar_width * self.current / self.total)
            bar = '█' * filled + '░' * (bar_width - filled)
            
            spinner = self.spinner_chars[self.spinner_idx]
            file_info = f" {Colors.GRAY}{current_file[:20]:<20}{Colors.RESET}" if current_file else ""
            
            progress_line = (
                f"\r{Colors.CYAN}{spinner}{Colors.RESET} "
                f"{Colors.BOLD}{self.desc}{Colors.RESET} "
                f"[{Colors.GREEN}{bar}{Colors.RESET}] "
                f"{Colors.YELLOW}{percent:5.1f}%{Colors.RESET} "
                f"({self.current}/{self.total})"
                f"{file_info}"
            )
            
            # Дополняем пробелами чтобы очистить остатки предыдущей строки
            progress_line = progress_line.ljust(120)
            
            sys.stdout.write(progress_line)
            sys.stdout.flush()
    
    def finish(self):
        print(f"\r{Colors.GREEN}✓{Colors.RESET} {self.desc} "
              f"[{Colors.GREEN}{'█' * 30}{Colors.RESET}] "
              f"{Colors.GREEN}100.0%{Colors.RESET} ({self.total}/{self.total})")


def compile_single_file(c_file):
    rel_parts = c_file.relative_to(ROOT / "src").parts
    rel_dir = Path(*rel_parts[:-1])
    obj_dir = BUILD_DIR / rel_dir
    ensure_dir(obj_dir)
    obj = obj_dir / (c_file.stem + ".o")
    run([CC] + CFLAGS + ["-c", str(c_file), "-o", str(obj)])
    return obj, c_file.name


def compile_kernel_c(num_workers):
    skip_dirs = {"bin", "recovery", "elf"}
    
    c_files = []
    for c_file in (ROOT / "src").rglob("*.c"):
        rel_parts = c_file.relative_to(ROOT / "src").parts
        if rel_parts[0] not in skip_dirs:
            c_files.append(c_file)
    
    print(f"{Colors.BLUE}[C]{Colors.RESET} Compiling kernel modules "
          f"{Colors.GRAY}(using {num_workers} cores){Colors.RESET}")
    
    objects = []
    progress = ProgressBar(len(c_files), "Compiling")
    
    with ThreadPoolExecutor(max_workers=num_workers) as executor:
        futures = {executor.submit(compile_single_file, cf): cf for cf in c_files}
        
        for future in as_completed(futures):
            c_file = futures[future]
            try:
                obj, filename = future.result()
                objects.append(obj)
                progress.update(filename)
            except Exception as e:
                print(f"\n{Colors.RED}Failed to compile {c_file}: {e}{Colors.RESET}")
                raise
    
    progress.finish()
    return objects


def print_step(step, message, color=Colors.BLUE):
    print(f"{color}[{step}]{Colors.RESET} {message}")


def compile_programs():
    print_step("PROGRAMS", "Compiling user ELF programs...")
    ensure_dir(APPS_SRC / "elf")
    elf_programs = [
        ("fileview.elf", ROOT / "src" / "elf" / "fileview.c"),
        ("keytest.elf", ROOT / "src" / "elf" / "keytest.c"),
        ("guess_game.elf", ROOT / "src" / "elf" / "guess_game.c"),
        ("gpf_test.elf", ROOT / "src" / "elf" / "gpf_test.c"),
        ("spin.elf", ROOT / "src" / "elf" / "spin.c"),
    ]
    
    for i, (name, src) in enumerate(elf_programs, 1):
        out = APPS_SRC / "elf" / name
        cmd = [
            CC, "-nostdlib", "-nostartfiles",
            f"-I{ROOT / 'include'}", "-N", "-static",
            f"-I{ROOT}", "-Ttext=0x1000000",
            "-o", str(out), str(src),
        ]
        run(cmd)
        print(f"  {Colors.GREEN}✓{Colors.RESET} [{i}/{len(elf_programs)}] {name}")


def compile_asm():
    print_step("ASM", "Compiling low-level assembly...")
    ensure_dir(BUILD_DIR)
    run([NASM, str(ROOT / "src" / "boot" / "kernel_entry.asm"), "-f", "elf32",
         "-o", str(BUILD_DIR / "kernel_entry.o")])
    print(f"  {Colors.GREEN}✓{Colors.RESET} kernel_entry.o")
    
    run([NASM, str(ROOT / "src" / "cpu" / "interrupt.asm"), "-f", "elf32",
         "-o", str(BUILD_DIR / "interrupt.o")])
    print(f"  {Colors.GREEN}✓{Colors.RESET} interrupt.o")


def compile_recovery():
    print_step("RECOVERY", "Compiling recovery module...")
    ensure_dir(RECOVERY_DIR)
    recovery_c = ROOT / "src" / "recovery" / "recovery.c"
    if not recovery_c.exists():
        raise FileNotFoundError(f"Recovery source not found: {recovery_c}")
    
    recovery_o = RECOVERY_DIR / "recovery.o"
    run([CC] + CFLAGS + ["-c", str(recovery_c), "-o", str(recovery_o)])
    print(f"  {Colors.GREEN}✓{Colors.RESET} recovery.o")
    
    font_obj = BUILD_DIR / "libc" / "font.o"
    if not font_obj.exists():
        raise FileNotFoundError(f"Font object not found: {font_obj}")
    
    recovery_elf = RECOVERY_DIR / "recovery.elf"
    run([LD] + LDFLAGS + ["--start-group", str(recovery_o), str(font_obj),
                          "--end-group", "-o", str(recovery_elf)])
    run([OBJCOPY, "-O", "binary", str(recovery_elf), str(BUILD_DIR / "recovery.bin")])
    print(f"  {Colors.GREEN}✓{Colors.RESET} recovery.bin")


def link_kernel():
    print_step("LD", "Linking kernel...")
    core_objs = [BUILD_DIR / "kernel_entry.o", BUILD_DIR / "interrupt.o"]
    for obj in sorted(BUILD_DIR.rglob("*.o")):
        name = obj.name
        if name in {"kernel_entry.o", "interrupt.o", "recovery.o"}:
            continue
        if obj not in core_objs:
            core_objs.append(obj)

    kernel_elf = BUILD_DIR / "kernel.elf"
    kernel_bin = BUILD_DIR / "kernel.bin"
    link_cmd = [LD] + LDFLAGS + ["--start-group"] + [str(o) for o in core_objs] + \
               ["--end-group", "-o", str(kernel_elf)]
    run(link_cmd)
    run([OBJCOPY, "-O", "binary", str(kernel_elf), str(kernel_bin)])
    print(f"  {Colors.GREEN}✓{Colors.RESET} kernel.bin")


def copy_apps():
    print_step("APPS", "Copying user programs...")
    ensure_dir(APPS_BUILD)
    if APPS_SRC.exists():
        shutil.copytree(APPS_SRC, APPS_BUILD, dirs_exist_ok=True)
    print(f"  {Colors.GREEN}✓{Colors.RESET} Applications copied")


def build_image():
    print_step("IMAGE", "Creating CawOS.img...")
    kernel_bin = BUILD_DIR / "kernel.bin"
    recovery_bin = BUILD_DIR / "recovery.bin"

    k_size = kernel_bin.stat().st_size
    r_size = recovery_bin.stat().st_size
    k_sectors = (k_size + 511) // 512
    r_sectors = (r_size + 511) // 512
    r_lba = 5 + k_sectors + 2

    print(f"  {Colors.GRAY}Kernel:{Colors.RESET} {k_size:,} bytes ({k_sectors} sectors)")
    print(f"  {Colors.GRAY}Recovery:{Colors.RESET} {r_size:,} bytes ({r_sectors} sectors), LBA: {r_lba}")

    run([NASM, str(ROOT / "src" / "boot" / "boot.asm"), "-f", "bin",
         "-o", str(BUILD_DIR / "boot.bin")])
    run([NASM, str(ROOT / "src" / "boot" / "stage2.asm"), "-f", "bin",
         f"-dKERNEL_SECTORS={k_sectors}",
         f"-dRECOVERY_SECTORS={r_sectors}",
         f"-dRECOVERY_LBA={r_lba}",
         "-o", str(BUILD_DIR / "stage2.bin")])

    run([sys.executable, str(ROOT / "scripts" / "build_img.py"),
         str(BUILD_DIR / "boot.bin"),
         str(BUILD_DIR / "stage2.bin"),
         str(kernel_bin),
         str(recovery_bin),
         str(APPS_BUILD),
         str(ROOT / "CawOS.img")])
    print(f"  {Colors.GREEN}✓{Colors.RESET} CawOS.img created")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Build CawOS")
    parser.add_argument("--clean", action="store_true", help="Clean build directory first")
    parser.add_argument("--no-programs", action="store_true", help="Skip compiling user ELF programs")
    parser.add_argument("-j", "--jobs", type=int, default=multiprocessing.cpu_count(),
                        help=f"Number of parallel jobs (default: {multiprocessing.cpu_count()})")
    args = parser.parse_args()

    if shutil.which(CC) is None or shutil.which(NASM) is None:
        print(f"{Colors.RED}[ERROR] i686-elf-gcc or nasm not found in PATH.{Colors.RESET}")
        sys.exit(1)

    print(f"\n{Colors.BOLD}{'=' * 50}{Colors.RESET}")
    print(f"{Colors.BOLD}        Building CawOS{Colors.RESET}")
    print(f"{Colors.BOLD}{'=' * 50}{Colors.RESET}\n")

    if args.clean:
        clean_build_dir()
    else:
        ensure_dir(BUILD_DIR)

    try:
        if not args.no_programs:
            compile_programs()
        compile_asm()
        compile_kernel_c(args.jobs)
        compile_recovery()
        link_kernel()
        copy_apps()
        build_image()
        
        print(f"\n{Colors.GREEN}{Colors.BOLD}✓ SUCCESS{Colors.RESET} {Colors.GREEN}CawOS.img is ready!{Colors.RESET}\n")
    except RuntimeError as e:
        print(f"\n{Colors.RED}{Colors.BOLD}✗ ERROR{Colors.RESET} {Colors.RED}{e}{Colors.RESET}\n")
        sys.exit(1)

if __name__ == "__main__":
    main()