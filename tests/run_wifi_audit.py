"""Compile and run the real audit module with host-only SDK type stubs."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=r"""From the workspace root (PowerShell):
  .\.venv\Scripts\python.exe tests/run_wifi_audit.py
  .\.venv\Scripts\python.exe tests/run_wifi_audit.py --cc C:\tools\zig\zig.exe

If no compiler is installed, install a pinned one outside the workspace:
  .\.venv\Scripts\python.exe -m pip install --target "$env:TEMP\usb-lab-test-compiler" ziglang==0.13.0

No firmware build, flash, COM access, Wi-Fi traffic, or automatic installation.
Builds run in a temporary directory and are deleted afterwards. Both signed-char
and unsigned-char builds run by default, with warnings as errors and undefined
behavior traps. The reduced SDK stub is test-only, not an SDK ABI validation.
""",
    )
    parser.add_argument("--cc", help="Path/name of Zig, Clang or GCC (not MSVC)")
    parser.add_argument("--char-mode", choices=("both", "signed", "unsigned"), default="both")
    args = parser.parse_args()
    zig = Path(tempfile.gettempdir()) / "usb-lab-test-compiler" / "ziglang" / "zig.exe"
    compiler = args.cc or shutil.which("zig") or (str(zig) if zig.is_file() else None)
    compiler = compiler or shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        parser.error("No host compiler found; see --help for temporary Zig installation.")
    command = [compiler]
    if Path(compiler).stem.lower() == "zig":
        command.append("cc")
    root = Path(__file__).resolve().parents[1]
    modes = ("signed", "unsigned") if args.char_mode == "both" else (args.char_mode,)
    try:
        subprocess.run([compiler, "version" if len(command) == 2 else "--version"], check=True)
        with tempfile.TemporaryDirectory(prefix="wifi-audit-tests-", ignore_cleanup_errors=True) as build:
            for mode in modes:
                exe = Path(build) / ("wifi_audit_" + mode + (".exe" if os.name == "nt" else ""))
                compile_command = command + [
                    "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2", "-g",
                    "-fsanitize=undefined", "-fsanitize-undefined-trap-on-error", f"-f{mode}-char",
                    "-I", str(root / "tests/stubs"), "-I", str(root / "src/modules"),
                    str(root / "tests/wifi_audit/test_wifi_audit.c"),
                    str(root / "src/modules/wifi_audit.c"), "-o", str(exe),
                ]
                # Production sources may lack final newlines; do not edit them.
                if len(command) == 2 or "clang" in Path(compiler).name.lower():
                    compile_command.append("-Wno-newline-eof")
                print(f"\nBUILD/RUN {mode}-char: {subprocess.list2cmdline(compile_command)}", flush=True)
                subprocess.run(compile_command, check=True, cwd=root)
                subprocess.run([str(exe)], check=True, cwd=root)
        print(f"SUCCESS: {len(modes)} host build(s) and test execution(s).", flush=True)
    except (OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Host tests failed: {error}\n")


if __name__ == "__main__":
    main()
