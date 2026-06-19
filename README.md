<div align="center">

# tinyC

**A small C compiler written in C. It lexes, parses, builds an IR, and emits
real x86-64 assembly. The goal is to make it compile itself.**

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
  ┌────────┐   ┌────────┐   ┌──────┐   ┌─────────┐   ┌──────┐
  │ lexer  │──▶│ parser │──▶│  IR  │──▶│ codegen │──▶│ emit │
  │ tokens │   │  AST   │   │      │   │ x86 AST │   │  .s  │
  └────────┘   └────────┘   └──────┘   └─────────┘   └──────┘
   token.c      parser.c     ir.c       codegen.c     emit.c
```

1. **Lexer** turns the source text into a stream of tokens.
2. **Parser** builds an AST with the usual operator precedence and associativity.
3. **IR** lowers the AST to a flat, temp-based intermediate form. This is where
   short-circuit logic and the like get turned into explicit branches.
4. **Codegen** turns the IR into an x86 AST, picking stack slots and registers.
5. **Emit** prints that as AT&T-syntax `.s` you can hand to an assembler.

You can stop after any stage to see what it produced (see [Build and run](#build-and-run)).

<details>
<summary><b>Module map</b></summary>

| Path | What it is |
|------|-----------|
| `src/main.c` | Driver: arg parsing and stage selection |
| `src/lexer/token.{c,h}` | Tokenizer |
| `src/parser/{parser,ast}.{c,h}` | Recursive-descent parser and AST |
| `src/ir/ir.{c,h}` | AST to IR lowering |
| `src/codegen/codegen.{c,h}` | IR to x86 AST |
| `src/codegen/x86/x86_ast.{c,h}` | x86 instruction model |
| `src/codegen/emit.{c,h}` | x86 AST to AT&T assembly text |
| `src/list.{c,h}`, `src/map.h`, `src/strlib/str.{c,h}` | Support data structures |

</details>

---

## What it handles today

It is a subset of C, not the whole language yet. Right now that covers:

- `int` variables, assignment, and `return`
- Functions (the driver and tests exercise multiple functions)
- `if`, and `void`
- Full expression support: arithmetic (`+ - * / %`), bitwise (`& | ^ ~ << >>`),
  comparison (`== != < > <= >=`), logical (`&& || !`) with short-circuit
  evaluation, unary minus and complement, and increment/decrement tokens
- Correct precedence, associativity, and unique labels for nested short-circuits

What is missing is most of the rest of the language. Closing that gap is the
road to self-hosting.

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
make test
```

There are unit tests for the list structure, the lexer, the parser, IR
generation, and codegen. Sample of the codegen and IR suite:

```
PASS: test_emit_binop_all_ops
PASS: test_emit_logical_and_short_circuit
PASS: test_emit_logical_or_short_circuit
PASS: test_emit_relational_ops
PASS: test_emit_nested_short_circuit_unique_labels
All IR tests passed!
```
