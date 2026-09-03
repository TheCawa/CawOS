#!/usr/bin/env python3

import os
import sys
import subprocess
import shutil
import glob
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"
APPS_SRC = ROOT / "src" / "bin"
APPS_BUILD = BUILD_DIR / "apps"
RECOVERY_DIR = BUILD_DIR / "recovery"
ELF_PROGRAMS = [
    ("fileview.elf", ROOT / "src" / "elf" / "fileview.c"),
    ("keytest.elf", ROOT / "src" / "elf" / "keytest.c"),
    ("guess_game.elf", ROOT / "src" / "elf" / "guess_game.c"),
    ("gpf_test.elf", ROOT / "src" / "elf" / "gpf_test.c"),
    ("spin.elf", ROOT / "src" / "elf" / "spin.c"),
]

CC = "i686-elf-gcc"
LD = "i686-elf-ld"
OBJCOPY = "i686-elf-objcopy"
NASM = "nasm"

CFLAGS = [
    "-march=i486",
    "-ffreestanding",
    "-fno-pie",
    "-fno-stack-protector",
    "-m32",
    f"-I{ROOT / 'include'}",
    "-Wall",
    "-O2",
    "-Wno-array-bounds",
]

LDFLAGS = ["-m", "elf_i386", "-T", str(ROOT / "scripts" / "linker.ld"), "-nostdlib"]


def run(cmd, cwd=None, check=True):
    print("  " + " ".join(str(c) for c in cmd))
    result = subprocess.run(cmd, cwd=cwd)
    if check and result.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(str(c) for c in cmd)}")
    return result


def ensure_dir(path):
    path.mkdir(parents=True, exist_ok=True)


def clean_build_dir():
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    ensure_dir(BUILD_DIR)


def compile_programs():
    print("[PROGRAMS] Compiling user ELF programs...")
    ensure_dir(APPS_SRC / "elf")
    for name, src in ELF_PROGRAMS:
        out = APPS_SRC / "elf" / name
        cmd = [
            CC,
            "-nostdlib",
            "-nostartfiles",
            f"-I{ROOT / 'include'}",
            "-N",
            "-static",
            f"-I{ROOT}",
            "-Ttext=0x1000000",
            "-o", str(out),
            str(src),
        ]
        run(cmd)


def compile_asm():
    print("[ASM] Compiling low-level assembly...")
    ensure_dir(BUILD_DIR)
    run([NASM, str(ROOT / "src" / "boot" / "kernel_entry.asm"), "-f", "elf32", "-o", str(BUILD_DIR / "kernel_entry.o")])
    run([NASM, str(ROOT / "src" / "cpu" / "interrupt.asm"), "-f", "elf32", "-o", str(BUILD_DIR / "interrupt.o")])


def compile_kernel_c():
    print("[C] Compiling kernel modules...")
    skip_dirs = {"bin", "recovery", "elf"}
    objects = []
    for c_file in sorted((ROOT / "src").rglob("*.c")):
        rel_parts = c_file.relative_to(ROOT / "src").parts
        if rel_parts[0] in skip_dirs:
            continue
        rel_dir = Path(*rel_parts[:-1])
        obj_dir = BUILD_DIR / rel_dir
        ensure_dir(obj_dir)
        obj = obj_dir / (c_file.stem + ".o")
        run([CC] + CFLAGS + ["-c", str(c_file), "-o", str(obj)])
        objects.append(obj)
    return objects


def compile_fs():
    print("[C] Compiling filesystem module with generated fs_config.h...")
    c_file = ROOT / "src" / "fs" / "fs.c"
    obj = BUILD_DIR / "fs" / "fs.o"
    ensure_dir(obj.parent)
    run([CC] + CFLAGS + ["-c", str(c_file), "-o", str(obj)])
    return obj


def compile_recovery():
    print("[RECOVERY] Compiling recovery module...")
    ensure_dir(RECOVERY_DIR)
    recovery_c = ROOT / "src" / "recovery" / "recovery.c"
    if not recovery_c.exists():
        raise FileNotFoundError(f"Recovery source not found: {recovery_c}")
    recovery_o = RECOVERY_DIR / "recovery.o"
    run([CC] + CFLAGS + ["-c", str(recovery_c), "-o", str(recovery_o)])
    font_obj = BUILD_DIR / "libc" / "font.o"
    if not font_obj.exists():
        raise FileNotFoundError(f"Font object not found: {font_obj}")
    print(f"  Using font object: {font_obj}")

    recovery_elf = RECOVERY_DIR / "recovery.elf"
    run([LD] + LDFLAGS + ["--start-group", str(recovery_o), str(font_obj), "--end-group", "-o", str(recovery_elf)])
    run([OBJCOPY, "-O", "binary", str(recovery_elf), str(BUILD_DIR / "recovery.bin")])


def link_kernel():
    print("[LD] Linking kernel...")
    core_objs = [BUILD_DIR / "kernel_entry.o", BUILD_DIR / "interrupt.o"]
    for obj in sorted(BUILD_DIR.rglob("*.o")):
        name = obj.name
        if name in {"kernel_entry.o", "interrupt.o", "recovery.o"}:
            continue
        if obj not in core_objs:
            core_objs.append(obj)

    kernel_elf = BUILD_DIR / "kernel.elf"
    kernel_bin = BUILD_DIR / "kernel.bin"
    link_cmd = [LD] + LDFLAGS + ["--start-group"] + [str(o) for o in core_objs] + ["--end-group", "-o", str(kernel_elf)]
    run(link_cmd)
    run([OBJCOPY, "-O", "binary", str(kernel_elf), str(kernel_bin)])


def copy_apps():
    print("[APPS] Copying user programs...")
    ensure_dir(APPS_BUILD)
    if APPS_SRC.exists():
        shutil.copytree(APPS_SRC, APPS_BUILD, dirs_exist_ok=True)


def build_image():
    print("[IMAGE] Creating CawOS.img...")
    kernel_bin = BUILD_DIR / "kernel.bin"
    recovery_bin = BUILD_DIR / "recovery.bin"

    k_size = kernel_bin.stat().st_size
    r_size = recovery_bin.stat().st_size
    k_sectors = (k_size + 511) // 512
    r_sectors = (r_size + 511) // 512
    r_lba = 5 + k_sectors + 2

    print(f"  Kernel size: {k_size} bytes ({k_sectors} sectors)")
    print(f"  Recovery size: {r_size} bytes ({r_sectors} sectors), LBA: {r_lba}")

    run([
        NASM,
        str(ROOT / "src" / "boot" / "boot.asm"),
        "-f", "bin",
        "-o", str(BUILD_DIR / "boot.bin"),
    ])
    run([
        NASM,
        str(ROOT / "src" / "boot" / "stage2.asm"),
        "-f", "bin",
        f"-dKERNEL_SECTORS={k_sectors}",
        f"-dRECOVERY_SECTORS={r_sectors}",
        f"-dRECOVERY_LBA={r_lba}",
        "-o", str(BUILD_DIR / "stage2.bin"),
    ])

    run([
        sys.executable,
        str(ROOT / "scripts" / "build_img.py"),
        str(BUILD_DIR / "boot.bin"),
        str(BUILD_DIR / "stage2.bin"),
        str(kernel_bin),
        str(recovery_bin),
        str(APPS_BUILD),
        str(ROOT / "CawOS.img"),
    ])


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Build CawOS")
    parser.add_argument("--clean", action="store_true", help="Clean build directory first")
    parser.add_argument("--no-programs", action="store_true", help="Skip compiling user ELF programs")
    args = parser.parse_args()

    if shutil.which(CC) is None or shutil.which(NASM) is None:
        print("[ERROR] i686-elf-gcc or nasm not found in PATH.")
        sys.exit(1)

    if args.clean:
        clean_build_dir()
    else:
        ensure_dir(BUILD_DIR)

    try:
        if not args.no_programs:
            compile_programs()
        compile_asm()
        compile_kernel_c()
        compile_recovery()
        link_kernel()
        copy_apps()
        build_image()
        link_kernel()
        build_image()
        print("\n[SUCCESS] CawOS.img is ready.")
    except RuntimeError as e:
        print(f"\n[ERROR] {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
