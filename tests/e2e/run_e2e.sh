#!/usr/bin/env bash
#
# End-to-end tests: compile each cases/*.c with tinyc all the way to a linked
# executable, run it, and check the process exit code against the expected value
# in the companion cases/<name>.expect file. (Expectations live in a separate
# file because the tinyc lexer has no comment support, so the .c fixtures must be
# pure C.) The same program is also compiled with the system cc and its exit code
# checked against the same expectation, so a wrong expectation is caught rather
# than silently agreeing with a tinyc bug.
#
# Usage: run_e2e.sh [path-to-tinyc]   (defaults to ../../tinyc next to this script)

set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tinyc="${1:-$script_dir/../../tinyc}"
cases_dir="$script_dir/cases"

if [ ! -x "$tinyc" ]; then
    echo "error: tinyc not found or not executable at: $tinyc" >&2
    echo "build it first (make) or pass its path as an argument." >&2
    exit 2
fi

cc_bin="${CC:-cc}"
have_cc=1
command -v "$cc_bin" >/dev/null 2>&1 || have_cc=0

pass=0
fail=0

# Run an executable and echo its exit status (works for codes 0-255).
run_status() {
    "$1" >/dev/null 2>&1
    echo $?
}

for src in "$cases_dir"/*.c; do
    name="$(basename "$src" .c)"

    expect_file="$cases_dir/$name.expect"
    expect="$(tr -cd '0-9' < "$expect_file" 2>/dev/null)"
    if [ -z "$expect" ]; then
        printf 'FAIL  %-32s (missing/empty %s.expect)\n' "$name" "$name"
        fail=$((fail + 1))
        continue
    fi

    # --- tinyc: source -> .s -> assemble/link -> run ---
    tc_exe="$cases_dir/$name"
    tc_err="$("$tinyc" "$src" 2>&1)"
    if [ $? -ne 0 ]; then
        printf 'FAIL  %-32s tinyc failed to compile: %s\n' "$name" "$(echo "$tc_err" | head -n1)"
        fail=$((fail + 1))
        rm -f "$tc_exe"
        continue
    fi
    tc_status="$(run_status "$tc_exe")"
    rm -f "$tc_exe"

    # --- cc: cross-check the expectation itself ---
    cc_note=""
    if [ "$have_cc" -eq 1 ]; then
        cc_exe="$cases_dir/$name.cc"
        if "$cc_bin" -std=c99 -w "$src" -o "$cc_exe" >/dev/null 2>&1; then
            cc_status="$(run_status "$cc_exe")"
            [ "$cc_status" = "$expect" ] || cc_note=" [cc=$cc_status disagrees with expect]"
        else
            cc_note=" [cc failed to compile]"
        fi
        rm -f "$cc_exe"
    fi

    if [ "$tc_status" = "$expect" ] && [ -z "$cc_note" ]; then
        printf 'PASS  %-32s exit=%s\n' "$name" "$tc_status"
        pass=$((pass + 1))
    else
        printf 'FAIL  %-32s tinyc=%s expect=%s%s\n' "$name" "$tc_status" "$expect" "$cc_note"
        fail=$((fail + 1))
    fi
done

echo "-----"
echo "e2e: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
