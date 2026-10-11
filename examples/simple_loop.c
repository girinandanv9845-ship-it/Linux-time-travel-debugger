#include <stdio.h>

int main(void) {
    printf("Starting loop execution...\n");
    for (int i = 0; i < 5; i++) {
        int val = i * 10;
        printf("Iteration %d: val = %d\n", i, val);
    }
    printf("Loop complete!\n");
    return 0;
}

