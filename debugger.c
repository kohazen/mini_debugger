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
        int status;
        wait(&status);
        printf("Debugger: Process started. Starting single-step...\n");

        while(WIFSTOPPED(status)) {
            ptrace(PTRACE_SINGLESTEP, child, NULL, NULL);
            wait(&status);
        }
        printf("Debugger: Target finished.\n");
    }
    return 0;
}
