#!/usr/bin/env python3
"""Pipe tests for alienfx_cli.

Each case starts the binary, writes bytes to its stdin, and checks the exit
code plus stdout and stderr. The checks are the command names the binary
prints. They do not open a device and do not name a laptop or a USB id.
"""

import subprocess
import sys


def run(binary, args, stdin):
    completed = subprocess.run(
        [binary, *args],
        input=stdin,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        check=False,
    )
    out = completed.stdout.decode("utf-8", "replace")
    err = completed.stderr.decode("utf-8", "replace")
    return completed.returncode, out, err


def expect(binary, args, stdin, code, out_has, err_has):
    status, out, err = run(binary, args, stdin)
    problems = []
    if code is None:
        if status == 0:
            problems.append("exit 0, expected a failure")
    elif status != code:
        problems.append(f"exit {status}, expected {code}")
    for needle in out_has:
        if needle not in out:
            problems.append(f"stdout missing {needle!r}")
    for needle in err_has:
        if needle not in err:
            problems.append(f"stderr missing {needle!r}")
    if problems:
        print(f"FAIL {' '.join(args) or '(no args)'}")
        for problem in problems:
            print(f"  {problem}")
        print("--- stdout ---")
        print(out)
        print("--- stderr ---")
        print(err)
        return 1
    print(f"ok {' '.join(args) or '(no args)'}")
    return 0


def main():
    if len(sys.argv) != 2:
        print("usage: pipe_tests.py <alienfx_cli>", file=sys.stderr)
        return 2
    binary = sys.argv[1]
    help_bits = (
        "setall",
        "setone",
        "setzone",
        "setdim",
        "status",
        "probe",
        "setkeys",
        "keyboardeffect",
        "powerbutton",
        "powerrainbow",
    )
    cases = [
        (["--help"], b"", 0, help_bits, ()),
        (["-h"], b"\n", 0, help_bits, ()),
        (["setall"], b"", None, (), ("required",)),
        (["setkeys"], b"", None, (), ("required",)),
    ]
    failed = 0
    for args, stdin, code, out_has, err_has in cases:
        failed += expect(binary, args, stdin, code, out_has, err_has)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
