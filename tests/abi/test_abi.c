/*
 * System V AMD64 conformance checks.
 *
 * These do not test tinyc. They pin the platform facts src/codegen is built on
 * -- argument registers, stack argument offsets, the return register, pushq's
 * operand width, and the 16-byte stack alignment requirement at a call -- by
 * driving hand-written assembly in abi_reference.s.
 *
 * The point is that when a codegen bug traces back to one of these, the
 * assumption itself has already been verified, so the argument is over before
 * it starts. If tinyc is ever ported to another platform, these fail first and
 * say exactly which assumption no longer holds.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef int (*ArgFn)(int, int, int, int, int, int, int, int);

int abi_arg0(int, int, int, int, int, int, int, int);
int abi_arg1(int, int, int, int, int, int, int, int);
int abi_arg2(int, int, int, int, int, int, int, int);
int abi_arg3(int, int, int, int, int, int, int, int);
int abi_arg4(int, int, int, int, int, int, int, int);
int abi_arg5(int, int, int, int, int, int, int, int);
int abi_arg6(int, int, int, int, int, int, int, int);
int abi_arg7(int, int, int, int, int, int, int, int);

unsigned long abi_push_mem_slot(void);

void abi_call_aligned(void);
void abi_call_misaligned(void);

// Argument N must arrive in the location codegen_instr writes it to. Reading
// each one back through %eax also confirms the return register.
static void test_argument_locations(void) {
    const ArgFn fns[] = { abi_arg0, abi_arg1, abi_arg2, abi_arg3,
                          abi_arg4, abi_arg5, abi_arg6, abi_arg7 };
    const char* where[] = { "%edi", "%esi", "%edx", "%ecx",
                            "%r8d", "%r9d", "16(%rbp)", "24(%rbp)" };

    for (int i = 0; i < 8; i++) {
        int got = fns[i](10, 11, 12, 13, 14, 15, 16, 17);
        int want = 10 + i;
        if (got != want) {
            printf("  FAIL: argument %d via %s -> %d, expected %d\n",
                   i, where[i], got, want);
            exit(1);
        }
    }
    printf("  PASS: test_argument_locations (%%edi %%esi %%edx %%ecx %%r8d %%r9d,"
           " then 16(%%rbp), 24(%%rbp))\n");
}

// pushq moves eight bytes, so pushing one of tinyc's 4-byte frame slots also
// reads the slot above it. This is why a stack argument held in memory has to
// be loaded into a register before being pushed.
static void test_push_reads_eight_bytes(void) {
    unsigned long pushed = abi_push_mem_slot();
    assert((pushed & 0xffffffffUL) == 0x11111111UL);
    if ((pushed >> 32) != 0x22222222UL) {
        printf("  FAIL: pushq of a 4-byte slot pushed 0x%016lx, expected the "
               "neighbouring slot in the high half\n", pushed);
        exit(1);
    }
    printf("  PASS: test_push_reads_eight_bytes (pushq -8(%%rbp) -> 0x%016lx)\n",
           pushed);
}

// Run fn in a child so a crash is observable instead of fatal. Returns the
// terminating signal, or 0 if the child exited normally.
static int signal_from(void (*fn)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        fn();
        // exit, not _exit: stdout is fully buffered when piped, and the child's
        // own output would otherwise be discarded. The parent flushed above, so
        // there is nothing inherited left to double-flush.
        exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) ? WTERMSIG(status) : 0;
}

// The ABI requires %rsp to be 16-byte aligned at the point the call executes.
// A correctly aligned call must work; that half is asserted. The misaligned
// half is reported rather than asserted, because whether it actually faults
// depends on which instructions the callee happens to use.
static void test_stack_alignment_at_call(void) {
    printf("    aligned call:\n");
    int aligned_signal = signal_from(abi_call_aligned);
    if (aligned_signal != 0) {
        printf("  FAIL: a correctly aligned call died with signal %d\n",
               aligned_signal);
        exit(1);
    }

    printf("    misaligned call (%%rsp 8 bytes off):\n");
    int misaligned_signal = signal_from(abi_call_misaligned);
    printf("  PASS: test_stack_alignment_at_call (aligned ok; misaligned %s)\n",
           misaligned_signal ? "died as expected" : "survived -- alignment bugs "
           "will not show up at runtime here, rely on check_call_asm.sh");
}

int main(void) {
    printf("Running System V AMD64 ABI conformance tests...\n");
    test_argument_locations();
    test_push_reads_eight_bytes();
    test_stack_alignment_at_call();
    printf("All ABI conformance tests passed!\n");
    return 0;
}
