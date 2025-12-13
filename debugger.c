#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>      // Defines struct user_regs_struct
#include <sys/uio.h>       // Required for struct iovec
#include <linux/elf.h>     // Required for NT_PRSTATUS
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main() {
    pid_t child = fork();

    if (child == 0) {
        // --- CHILD (Target) ---
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl("./target", "target", NULL);

    } else {
        // --- PARENT (Debugger) ---
        int status;
        wait(&status);
        printf("Debugger: Loaded (ARM64 Mode). PID: %d\n", child);

        // 1. Get Input
        unsigned long addr;
        printf("Enter address to breakpoint (e.g., 0x400000...): ");
        scanf("%lx", &addr);

        // 2. Set Breakpoint (Req 2)
        // Read 8 bytes (long), but we will only use the lower 4 bytes for the instruction.
        unsigned long data = ptrace(PTRACE_PEEKTEXT, child, (void*)addr, NULL);
        unsigned long original_data = data;

        // Create the trap: 0xd4200000 is "BRK #0" in ARM64 assembly
        // We preserve the top 32 bits and swap the bottom 32 bits with the trap.
        unsigned long trap_data = (data & 0xFFFFFFFF00000000) | 0xd4200000;

        ptrace(PTRACE_POKETEXT, child, (void*)addr, (void*)trap_data);
        printf("Debugger: Breakpoint set at 0x%lx (Instruction: BRK #0)\n", addr);

        // 3. Continue to Breakpoint
        ptrace(PTRACE_CONT, child, NULL, NULL);
        wait(&status);

        // 4. Handle Breakpoint
        if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
            printf("\nDebugger: Breakpoint HIT!\n");

            // Restore original instruction (Req 2)
            ptrace(PTRACE_POKETEXT, child, (void*)addr, (void*)original_data);

            // Get Registers
            // We use 'struct user_regs_struct' which is standard in <sys/user.h>
            struct user_regs_struct regs;
            struct iovec iov;
            iov.iov_base = &regs;
            iov.iov_len = sizeof(regs);
            
            // On ARM64, we must use PTRACE_GETREGSET
            ptrace(PTRACE_GETREGSET, child, NT_PRSTATUS, &iov);

            // Ensure the Program Counter (pc) is pointing to the breakpoint address
            if (regs.pc != addr) {
                regs.pc = addr;
                ptrace(PTRACE_SETREGSET, child, NT_PRSTATUS, &iov);
            }
            
            printf("Debugger: Code restored. Entering Step Mode at PC: 0x%llx\n", regs.pc);

            // 5. Resume Stepping (Req 3)
            while(WIFSTOPPED(status)) {
                ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
                wait(&status);
                
                // Read regs again to see where we are
                ptrace(PTRACE_GETREGSET, child, NT_PRSTATUS, &iov);
                printf("Stepping at PC: 0x%llx\n", regs.pc);
            }
        }
        printf("Debugger: Target finished.\n");
    }
    return 0;
}