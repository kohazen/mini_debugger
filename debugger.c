#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    pid_t child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl("./target", "target", NULL);
    } else {
        wait(NULL); // Wait for start
        printf("Debugger: Process started. Running to completion...\n");
        ptrace(PTRACE_CONT, child, NULL, NULL);
    }
    return 0;
}
