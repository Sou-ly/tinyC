#!/usr/bin/env bash
#
# Static checks on the assembly tinyc generates for function calls.
#
# Runtime tests (run_e2e.sh) only catch an ABI violation if it happens to crash
# or corrupt something observable. A misaligned %rsp at a `call` usually does
# neither -- until the callee is a libc function that uses aligned SSE moves,
# at which point it segfaults somewhere unrelated. So instead of hoping for a
# crash, walk the emitted .s and verify the invariants directly.
#
# Checked per function body:
#   1. no leftover pseudo-registers (`<pseudo:...>`) or unknown mnemonics (`???`)
#   2. the frame allocation `subq $N, %rsp` is a multiple of 16
#   3. no `pushq` of a memory operand -- pushq moves 8 bytes, so pushing a
#      4-byte slot like -4(%rbp) also reads its neighbour
#   4. %rsp is 16-byte aligned at every `call`
#   5. bytes pushed for arguments are all reclaimed afterwards, so a later call
#      in the same function still starts from an aligned %rsp
#   6. no two-operand `imul` with a memory destination -- unlike add/sub/and,
#      imul has no register-to-memory form
#
# Every generated .s is also handed to the assembler, which catches any illegal
# form the rules above don't know about yet.
#
# Check 4/5 track %rsp by walking straight-line code, anchoring at
# `movq %rsp, %rbp` in the prologue (where %rsp is aligned) and resetting at
# `movq %rbp, %rsp` in the epilogue. Argument setup is always straight-line, so
# this is accurate for the code tinyc emits; a report that looks like a false
# positive means the arg-setup sequence spans a branch, which is worth a look
# regardless.
#
# Usage: check_call_asm.sh [path-to-tinyc]

set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tinyc="${1:-$script_dir/../../tinyc}"
cases_dir="$script_dir/cases"

if [ ! -x "$tinyc" ]; then
    echo "error: tinyc not found or not executable at: $tinyc" >&2
    exit 2
fi

pass=0
fail=0

for src in "$cases_dir"/*.c; do
    name="$(basename "$src" .c)"
    asm="$cases_dir/$name.s"

    rm -f "$asm"
    if ! err="$("$tinyc" --codegen "$src" 2>&1)" || [ ! -f "$asm" ]; then
        printf 'FAIL  %-32s tinyc --codegen failed: %s\n' "$name" "$(echo "$err" | head -n1)"
        fail=$((fail + 1))
        rm -f "$asm"
        continue
    fi
    # ICE writes to stderr but still exits 0, so check the message too.
    if echo "$err" | grep -q 'internal compiler error'; then
        printf 'FAIL  %-32s %s\n' "$name" "$(echo "$err" | head -n1)"
        fail=$((fail + 1))
        rm -f "$asm"
        continue
    fi

    problems="$(awk '
    function flush_fn() {
        if (fn != "" && pushed != reclaimed)
            printf "  %s: %d bytes of arg pushes/padding, %d reclaimed\n", fn, pushed, reclaimed
    }
    # function label: a bare symbol, not a .L local label
    /^[A-Za-z_][A-Za-z0-9_]*:[ \t]*$/ {
        flush_fn()
        fn = substr($0, 1, index($0, ":") - 1)
        delta = 0; pushed = 0; reclaimed = 0; seen_frame = 0
        next
    }
    /<pseudo:/  { printf "  %s:%d: unresolved pseudo-register: %s\n", fn, NR, $0 }
    /\?\?\?/    { printf "  %s:%d: unknown mnemonic or register: %s\n", fn, NR, $0 }

    /movq[ \t]+%rsp,[ \t]*%rbp/ { delta = 0; next }   # prologue: %rsp is aligned here
    /movq[ \t]+%rbp,[ \t]*%rsp/ { delta = 0; next }   # epilogue: frame discarded

    /pushq/ {
        if ($0 ~ /%rbp[ \t]*$/) next                  # prologue push, part of the anchor
        if ($0 ~ /\(/) printf "  %s:%d: pushq of a memory operand reads 8 bytes: %s\n", fn, NR, $0
        delta -= 8; pushed += 8
        next
    }
    /subq[ \t]+\$[0-9]+,[ \t]*%rsp/ {
        n = $0; sub(/^.*\$/, "", n); sub(/,.*$/, "", n); n += 0
        if (!seen_frame) {
            seen_frame = 1
            if (n % 16 != 0)
                printf "  %s:%d: frame size %d is not a multiple of 16\n", fn, NR, n
        } else {
            pushed += n                               # explicit alignment padding
        }
        delta -= n
        next
    }
    /addq[ \t]+\$[0-9]+,[ \t]*%rsp/ {
        n = $0; sub(/^.*\$/, "", n); sub(/,.*$/, "", n); n += 0
        delta += n; reclaimed += n
        next
    }
    # imul (2-operand) has no register-to-memory form, unlike add/sub/and.
    /^[ \t]*imul[bwlq]?[ \t]+/ {
        operands = $0; sub(/^[ \t]*imul[bwlq]?[ \t]+/, "", operands)
        if (split(operands, parts, ",") == 2 && parts[2] ~ /\(/)
            printf "  %s:%d: imul has no register-to-memory form: %s\n", fn, NR, $0
        next
    }
    /^[ \t]*callq?[ \t]+/ {
        target = $0; sub(/^[ \t]*callq?[ \t]+/, "", target); sub(/[ \t]*$/, "", target)
        if (target == "")
            printf "  %s:%d: call with no target\n", fn, NR
        if (delta % 16 != 0)
            printf "  %s:%d: %%rsp misaligned by %d bytes at call to %s\n", \
                   fn, NR, ((delta % 16) + 16) % 16, target
        next
    }
    END { flush_fn() }
    ' "$asm")"

    # Catch-all: anything the rules above miss, the assembler still rejects.
    if ! as_err="$(as -o /dev/null "$asm" 2>&1)"; then
        problems="$problems
$(echo "$as_err" | grep -i 'error' | sed 's/^/  /' | head -n5)"
    fi

    rm -f "$asm"

    if [ -z "${problems//[$' \t\n']/}" ]; then
        printf 'PASS  %-32s\n' "$name"
        pass=$((pass + 1))
    else
        printf 'FAIL  %-32s\n%s\n' "$name" "$problems"
        fail=$((fail + 1))
    fi
done

echo "-----"
echo "asm checks: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
