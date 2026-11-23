# Agents.md — MathC Toolchain & Contributor Guide

## Architecture Overview
- `src/mcc.c`: Self-contained C99 driver, lexer/parser, direct interpreter, and C/object-file emitter.
- `config.h`: Cross-platform OS/compiler detection and Windows Terminal setup macros.
- `configure` & `Makefile.in`: POSIX build configuration generating `Makefile` for Linux, macOS, and Android.
- `examples/*.mc`: Canonical MathC test scripts.

## Rules for Automated & Human Maintainers
1. Keep all source code, CLI output, error messages, and documentation strictly in English.
2. Preserve exact compatibility with the `mcc` CLI flags (`run`, `-o`, `--compiler`, `--command`, `--help`, `--readme`, `--manual`, `--license`, `--agents`, `--update`).
3. Keep `src/mcc.c` C99-compliant with zero external runtime dependencies beyond the standard C library and `<math.h>` (`-lm`).
4. Verify `./configure && make test` after every change to the C compiler or interpreter.
