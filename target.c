#include <stdio.h>
void secret_function() {
    printf("Secret function hit!\n");
}
int main() {
    printf("Target: Started. Address of secret: %p\n", secret_function);
    secret_function();
    return 0;
}
