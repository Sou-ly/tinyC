<div align="center">

# tinyC

**A small C compiler written in C. It lexes, parses, typechecks, builds an IR,
and emits real x86-64 assembly. The goal is to make it compile itself.**

[![Language](https://img.shields.io/badge/written%20in-C-555555?logo=c)](src/)
[![Build](https://img.shields.io/badge/build-make-1F6FEB)](Makefile)
[![Target](https://img.shields.io/badge/target-x86--64%20(AT%26T)-CE422B)](src/codegen)
[![Tests](https://img.shields.io/badge/tests-passing-3FB950)](#tests)

</div>

---

```
   C in                                            x86-64 out
  ──────                                           ──────────
  int main() {                                     _main:
      int x = 2 + 3 * 4;        tinyc                 movl  $3, -4(%rbp)
      return x;             ───────────▶              imull $4, -4(%rbp)
  }                          lex, parse,              movl  $2, -8(%rbp)
                             ir, codegen              addl  %r10d, -8(%rbp)
                                                      ...
                                                      movl  -12(%rbp), %eax
                                                      ret
```

Writing a compiler in the language it compiles is a nice forcing function. Every
feature I add is one I can then use inside the compiler itself, and "can it build
itself yet?" is a very concrete way to track progress.

---

## How it works

Standard compiler pipeline, one stage per directory:

```
  source.c                                                out.s
     │                                                      ▲
     ▼                                                      │
  ┌────────┐   ┌────────┐   ┌──────────┐   ┌──────┐   ┌─────────┐   ┌──────┐
  │ lexer  │──▶│ parser │──▶│ semantic │──▶│  IR  │──▶│ codegen │──▶│ emit │
  │ tokens │   │  AST   │   │ analysis │   │      │   │ x86 AST │   │  .s  │
  └────────┘   └────────┘   └──────────┘   └──────┘   └─────────┘   └──────┘
   token.c      parser.c    resolve +       ir.c       codegen.c     emit.c
                             typecheck
```

1. **Lexer** turns the source text into a stream of tokens.
2. **Parser** builds an AST with the usual operator precedence and associativity.
3. **Semantic analysis** resolves variables to unique identifiers (catching
   redeclarations), validates function calls and parameter counts, and resolves
   labels for `goto` and loops.
4. **IR** lowers the AST to a flat, temp-based intermediate form. This is where
   short-circuit logic and the like get turned into explicit branches.
5. **Codegen** turns the IR into an x86 AST, picking stack slots and registers.
6. **Emit** prints that as AT&T-syntax `.s` you can hand to an assembler.

You can stop after any stage to see what it produced (see [Build and run](#build-and-run)).

<details>
<summary><b>Module map</b></summary>

| Path | What it is |
|------|-----------|
| `src/main.c` | Driver: arg parsing and stage selection |
| `src/lexer/token.{c,h}` | Tokenizer |
| `src/parser/{parser,ast}.{c,h}` | Recursive-descent parser, semantic passes, and AST |
| `src/ir/ir.{c,h}` | AST to IR lowering |
| `src/codegen/codegen.{c,h}` | IR to x86 AST |
| `src/codegen/x86/x86_ast.{c,h}` | x86 instruction model |
| `src/codegen/emit.{c,h}` | x86 AST to AT&T assembly text |
| `src/strlib/str.{c,h}` | String utilities |
| `src/common/list.h` | Generic dynamic array (macro-based) |
| `src/common/optional.h` | Optional type wrapper |
| `src/common/ice.h` | Internal compiler error macro |

</details>

---

## What it handles today

It is a subset of C, not the whole language yet. Right now that covers:

- `int` variables, assignment, and `return`
- `void` as a return type
- Functions: declarations, definitions with parameters, and calls
- Control flow: `if`/`else`, `for`, `while`, `do`/`while`, `break`, `continue`
- `switch`/`case`/`default` with fallthrough
- `goto` and labels
- Ternary conditional (`? :`)
- Full expression support: arithmetic (`+ - * / %`), bitwise (`& | ^ ~ << >>`),
  comparison (`== != < > <= >=`), logical (`&& || !`) with short-circuit
  evaluation, unary minus and complement, and prefix/postfix increment/decrement
- Compound assignment (`+= -= *= /= %= &= |= ^= <<= >>=`)
- Correct precedence, associativity, and unique labels for nested short-circuits
- Semantic analysis: variable resolution, typechecking (function arity), label
  resolution for `goto` and loop `break`/`continue`

What is missing is most of the rest of the language: pointers, arrays, structs,
strings, multiple types, and the preprocessor. Closing that gap is the road to
self-hosting.

---

## See it work

```c
// sample.c
int main() {
    int x = 2 + 3 * 4;
    return x;
}
```

```bash
make
./tinyc --codegen sample.c   # writes sample.s next to the source
```

```asm
.global _main
_main:
    pushq %rbp
    movq  %rsp, %rbp
    subq  $12, %rsp
    movl  $3, -4(%rbp)
    imull $4, -4(%rbp)
    movl  $2, -8(%rbp)
    movl  -4(%rbp), %r10d
    addl  %r10d, -8(%rbp)
    movl  -8(%rbp), %r10d
    movl  %r10d, -12(%rbp)
    movl  -12(%rbp), %eax
    movq  %rbp, %rsp
    popq  %rbp
    ret
```

---

## Build and run

```bash
make                       # builds ./tinyc

./tinyc sample.c           # full pipeline, emits sample.s
./tinyc --lex     file.c   # stop after lexing
./tinyc --parse   file.c   # stop after parsing
./tinyc --ir      file.c   # stop after IR
./tinyc --codegen file.c   # stop after codegen (emit .s)
./tinyc --help
```

The input file has to end in `.c`.

---

## Tests

```bash
make test        # unit tests
make test-e2e    # end-to-end tests (compile, assemble, run, check exit code)
```

Unit tests cover the list structure, lexer, parser, AST, IR generation, codegen,
variable scoping, and typechecking. End-to-end tests compile small C programs
through the full pipeline, run the resulting binary, and verify the exit code
against the system compiler.

```
PASS: test_emit_binop_all_ops
PASS: test_emit_logical_and_short_circuit
PASS: test_emit_logical_or_short_circuit
PASS: test_emit_relational_ops
PASS: test_emit_nested_short_circuit_unique_labels
All IR tests passed!
```
