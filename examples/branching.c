#include <stdio.h>

int main(void) {
    int threshold = 25;
    int input = 30;

    if (input > threshold) {
        printf("Branch Taken: input %d exceeds threshold %d\n", input, threshold);
    } else {
        printf("Branch Not Taken: input %d is within threshold %d\n", input, threshold);
    }

    return 0;
}

