# MathC Language Reference Manual (v1.0.0)

## 1. Lexical Elements
- Comments begin with `#` or `//` and continue to the end of the line.
- Identifiers for variables and sets are alphanumeric (`A`, `B`, `x`, `theta`).
- Supported arithmetic operators: `+`, `-`, `*`, `x`, `×`, `/`, `÷`, `^`, `%`.
- Supported comparison operators: `<`, `>`, `<=`, `=<`, `>=`, `=>`, `=`, `==`, `!=`, `<>`.

## 2. Statements

### 2.1 `calculate`
Syntax: `calculate <first> <operator> <second>`
Evaluates numeric operations (`calculate 5 x 5` -> `25`, `calculate 1 + 1` -> `2`) as well as symbolic trigonometric identities (`calculate cos + cos` -> `2*cos`, `calculate sin + sin` -> `2*sin`).

### 2.2 `set of number`
Roster syntax:
  `set of number A = {1; 2; 3; 4; 5}`
Set-builder syntax:
  `set of number A = { x < 10 | x is N* }`
  `set of number B = { x < 5 | x is Z }`
  `set of number C = { -3 <= x <= 3 | x is Z }`
Supported domains:
  - `N`: non-negative integers `{0; 1; 2; ...}`
  - `N*`: positive integers `{1; 2; 3; ...}`
  - `Z`: integers `{...; -2; -1; 0; 1; 2; ...}`
  - `even`, `odd`, `prime`: filtered integer subsets

### 2.3 `compute`
Syntax: `compute <expression>`
Evaluates parenthesized mathematical expressions with standard precedence (`^`, then `*`/`x`/`÷`/`/`/`%`, then `+`/`-`).
Functions: `sin`, `cos`, `tan`, `sqrt`, `abs`, `log`, `ln`, `floor`, `ceil`, `round`.
Constants: `pi`, `e`.

### 2.4 `compare`
Syntax: `compare <lhs> <op> <rhs>`
Compares scalars or compares a scalar against every element of a defined set (`compare 5 =< A`, `compare 3 >= B`).

### 2.5 Extended Mathematical Commands
- `let <name> = <expr>`: Bind a scalar numeric variable.
- `solve <equation>`: Solve linear (`2x + 4 = 10`) or quadratic (`x^2 - 5x + 6 = 0`) equations.
- `factor <integer>`: Compute prime factorization (`factor 360` -> `2^3 * 3^2 * 5`).
- `gcd <a> <b>` / `lcm <a> <b>`: Greatest common divisor / least common multiple.
- `prime <n>`: Primality test.
- `union <A> <B>` / `intersect <A> <B>` / `diff <A> <B>`: Set algebra.
- `stats <A>`: Count, sum, mean, min, max, and standard deviation of a set.
- `derivative <poly>`: Symbolic derivative of polynomial/trig terms.
- `integrate <expr> from <a> to <b>`: Definite numerical integral via Simpson's rule.

## 3. Compilation Model
`mcc <file.mc> -o <file.o>` lowers MathC AST statements to C99/HolyC-compatible intermediate source and invokes the system C compiler (`gcc`/`clang`) with `-c` to emit a native relocatable object file (`.o` / `.obj`), or links a standalone native executable when `-o` does not end in `.o`.
