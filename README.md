# MathC

MathC is a mathematical programming language and native toolchain (`mcc`) that lets you express arithmetic, symbolic trigonometry, set theory, algebraic equations, and calculus in clean English syntax—and either run scripts directly or compile them to C/HolyC-style object files (`.o`) and native executables.

## Core Commands

- `calculate <lhs> <op> <rhs>` — Evaluate binary or symbolic operations (`calculate 1 + 1`, `calculate 5 x 5`, `calculate cos + cos`, `calculate sin + sin`)
- `set of number <Name> = { ... }` — Define explicit sets (`{1; 2; 3}`) or set-builder domains (`{ x < 10 | x is N* }`, `{ x < 5 | x is Z }`)
- `compute <expr>` — Evaluate nested expressions with `()`, `+`, `-`, `x`, `*`, `÷`, `/`, `^`, `%`, and functions (`sin`, `cos`, `tan`, `sqrt`, `abs`, `log`, `ln`)
- `compare <lhs> <op> <rhs>` — Compare numbers and sets (`<`, `>`, `<=`, `=<`, `>=`, `=>`, `=`, `!=`)
- Extended Math: `let`, `solve`, `factor`, `gcd`, `lcm`, `prime`, `union`, `intersect`, `diff`, `stats`, `derivative`, `integrate`, `matrix`, `det`, `print`

## CLI Usage (`/usr/local/bin/mcc`)

```text
Usage: mcc <option> [arg]
Example: mcc run <file_path>: Run file
         mcc <file_path> -o <output-file>: Compile file
         mcc --compiler <file_path> -o <output-file>: Compile file
         mcc --command <command>: Run command
         mcc --help: Show usage instructions
         mcc --readme: Show readme
         mcc --manual: Show manual
         mcc --license: Show MIT license
         mcc --agents: Show Agents.md
         mcc --update: Update or reload
```

## Building from Source

### Linux / macOS / Android (Termux)
```sh
./configure
make
sudo make install
```

### Windows Terminal
Include `config.h` and compile with MinGW or MSVC, then copy `mcc.exe` into `%LOCALAPPDATA%\MathC\bin` and add it to your user `Path`.
