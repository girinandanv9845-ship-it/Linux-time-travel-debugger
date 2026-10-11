#include <stdio.h>

int multiply_add(int x, int y, int z) {
    int temp = x * y;
    return temp + z;
}

int compute(int val) {
    int step1 = multiply_add(val, 2, 5);
    int step2 = multiply_add(step1, 3, 10);
    return step2;
}

int main(void) {
    int res = compute(4);
    printf("Computation result: %d\n", res);
    return 0;
}

