# End-to-end tests

Each `cases/<name>.c` is compiled by `tinyc` all the way to a linked executable,
run, and its process **exit code** is checked against `cases/<name>.expect`.
Every program is also compiled with the system `cc` and run; if `cc`'s exit code
disagrees with the `.expect` value, the case fails too — so a wrong expectation
is caught instead of silently matching a `tinyc` bug.

Run with:

```
make test-e2e            # builds tinyc, then runs the suite
./tests/e2e/run_e2e.sh   # if tinyc is already built (optional path arg)
```

## Why expectations live in a separate file

The `tinyc` lexer has no comment support, so the `.c` fixtures must be pure C.
The expected exit code therefore lives in a companion `<name>.expect` file
(a single integer) rather than a comment in the source.

## Cases

| Case | Exercises | Exit |
| --- | --- | --- |
| `fallthrough` | control falls through clauses with no `break` | 7 |
| `break_terminates` | `break` ends the switch | 1 |
| `default_taken` | `default` runs when no case matches | 7 |
| `default_middle_fallthrough` | mid-list `default`, then falls into the next case | 13 |
| `no_match_no_default` | no match, no default → body skipped | 5 |
| `nested` | switch nested in a switch; inner `break` is local | 21 |
| `switch_in_loop_break` | `break` exits the switch, not the enclosing loop | 8 |
| `switch_in_loop_continue` | `continue` belongs to the enclosing loop | 48 |
| `switch_on_expression` | controlling expression evaluated once | 2 |
| `relational_ops` | regression guard for the `cmp` operand-order fix | 13 |

Exit codes are the process status (0–255), so keep expected values small.
