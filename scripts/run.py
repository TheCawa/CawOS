#!/usr/bin/env python3
"""
Run CawOS in QEMU with serial output capture.
"""
import os
import sys
import subprocess
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IMAGE = ROOT / "CawOS.img"
DEFAULT_LOG = ROOT / "serial.log"

QEMU = "qemu-system-i386"


def build_qemu_args(args):
    qemu_args = [
        QEMU,
        "-drive", f"format=raw,file={IMAGE},if=ide,index=0,media=disk",
        "-m", str(args.memory),
        "-vga", "std",
        "-serial", args.serial,
    ]
    if args.headless:
        qemu_args.append("-nographic")
    if args.no_audio:
        pass
    else:
        qemu_args.extend([
            "-audiodev", "driver=dsound,id=snd0",
            "-device", "ac97,audiodev=snd0",
        ])
    if args.extra:
        qemu_args.extend(args.extra)
    return qemu_args


def run_foreground(qemu_args):
    print("Starting CawOS...")
    print("  " + " ".join(qemu_args))
    try:
        result = subprocess.run(qemu_args)
        return result.returncode
    except FileNotFoundError:
        print(f"[ERROR] {QEMU} not found in PATH.")
        return 1


def run_with_timeout(qemu_args, timeout, log_path):
    print(f"Starting CawOS with {timeout}s timeout, logging serial to {log_path}...")
    print("  " + " ".join(qemu_args))
    with open(log_path, "wb", buffering=0) as log_file:
        try:
            subprocess.run(qemu_args, stdout=log_file, stderr=subprocess.STDOUT, timeout=timeout)
        except FileNotFoundError:
            print(f"[ERROR] {QEMU} not found in PATH.")
            return 1
        except subprocess.TimeoutExpired:
            return 0
    return 0


def main():
    parser = argparse.ArgumentParser(description="Run CawOS in QEMU")
    parser.add_argument("--memory", type=int, default=256, help="RAM in MB (default: 256)")
    parser.add_argument("--serial", default="stdio", help="QEMU serial backend (default: stdio)")
    parser.add_argument("--headless", action="store_true", help="Run with -nographic")
    parser.add_argument("--no-audio", action="store_true", help="Disable AC97 audio")
    parser.add_argument("--timeout", type=float, default=0, help="Run for N seconds then terminate (0 = run forever)")
    parser.add_argument("--log", type=str, default=str(DEFAULT_LOG), help="Serial log file for timeout mode")
    parser.add_argument("--extra", nargs="*", help="Extra QEMU arguments")
    args = parser.parse_args()

    if not IMAGE.exists():
        print(f"[ERROR] {IMAGE} not found! Run scripts/build.py first.")
        sys.exit(1)

    qemu_args = build_qemu_args(args)

    if args.timeout > 0:
        sys.exit(run_with_timeout(qemu_args, args.timeout, args.log))
    else:
        sys.exit(run_foreground(qemu_args))


if __name__ == "__main__":
    main()
