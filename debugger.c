#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>      // Defines struct user_regs_struct
#include <sys/uio.h>       // Required for struct iovec
#include <linux/elf.h>     // Required for NT_PRSTATUS
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>


/* --- addition: error handling macro --- */
#define CHECK(call) do { if ((call) == -1) { perror(#call); exit(1); } } while (0)

/* --- addition: explicit status printing --- */
void print_status(int status) {
    if (WIFSTOPPED(status))
        printf("Process stopped by signal %d\n", WSTOPSIG(status));
    else if (WIFEXITED(status))
        printf("Process exited with code %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("Process terminated by signal %d\n", WTERMSIG(status));
}

int main() {
    pid_t child = fork();

    if (child == 0) {
        // --- CHILD (Target) ---
        /* Updated: add basic error handling */
        CHECK(ptrace(PTRACE_TRACEME, 0, NULL, NULL));
        CHECK(execl("./target", "target", NULL));

        /* execl returns only on failure */
        exit(1);

    } else {
        // --- PARENT (Debugger) ---
        int status;
        /* Updated: check wait() and show process status */
        CHECK(wait(&status));
        print_status(status);
        printf("Debugger: Loaded (ARM64 Mode). PID: %d\n", child);

        // 1. Get Input
        unsigned long addr;
        printf("Enter address to breakpoint (e.g., 0x400000...): ");
        /* Updated: validate user input */
        if (scanf("%lx", &addr) != 1) {
            fprintf(stderr, "Invalid address input\n");
            exit(1);
        }

        // 2. Set Breakpoint (Req 2)
        /* Updated: handle ptrace error explicitly */
        errno = 0;
        unsigned long data = ptrace(PTRACE_PEEKTEXT, child, (void*)addr, NULL);
        if (errno) {
            perror("PTRACE_PEEKTEXT");
            exit(1);
        }
        unsigned long original_data = data;

        // Create the trap: 0xd4200000 is "BRK #0" in ARM64 assembly
        // We preserve the top 32 bits and swap the bottom 32 bits with the trap.
        unsigned long trap_data = (data & 0xFFFFFFFF00000000UL) | 0xd4200000;
        /* Updated: error-checked write */
        CHECK(ptrace(PTRACE_POKETEXT, child, (void*)addr, (void*)trap_data));
        printf("Debugger: Breakpoint set at 0x%lx (Instruction: BRK #0)\n", addr);

        // 3. Continue to Breakpoint
        /* Updated: error checks + status reporting */
        CHECK(ptrace(PTRACE_CONT, child, NULL, NULL));
        CHECK(wait(&status));
        print_status(status);

        // 4. Handle Breakpoint
        if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
            printf("\nDebugger: Breakpoint HIT!\n");
            // Restore original instruction (Req 2)
            /* Updated: explicit breakpoint removal */
            CHECK(ptrace(PTRACE_POKETEXT, child, (void*)addr, (void*)original_data));

            // Get Registers
            // We use 'struct user_regs_struct' which is standard in <sys/user.h>
            struct user_regs_struct regs;
            struct iovec iov;
            iov.iov_base = &regs;
            iov.iov_len = sizeof(regs);
            /* Updated: error-checked register fetch */
            CHECK(ptrace(PTRACE_GETREGSET, child, NT_PRSTATUS, &iov));

            // Ensure the Program Counter (pc) is pointing to the breakpoint address
            // BRK advances PC on ARM64, so we rewind it
            if (regs.pc != addr) {
                regs.pc = addr;
                CHECK(ptrace(PTRACE_SETREGSET, child, NT_PRSTATUS, &iov));
            }

            printf("Debugger: Code restored. Entering Step Mode at PC: 0x%llx\n", regs.pc);

            // 5. Resume Stepping (Req 3)
            /* Updated: re-evaluate status after each wait() */
            do {
                CHECK(ptrace(PTRACE_SINGLESTEP, child, NULL, NULL));
                CHECK(wait(&status));
                print_status(status);

                CHECK(ptrace(PTRACE_GETREGSET, child, NT_PRSTATUS, &iov));
                printf("Stepping at PC: 0x%llx\n", regs.pc);

            } while (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP);
        }

        printf("Debugger: Target finished.\n");
    }
    return 0;
}
