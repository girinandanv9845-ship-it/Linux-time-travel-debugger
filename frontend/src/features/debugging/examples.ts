import { ExampleProgram } from '../../types/debugger';

export const EXAMPLE_PROGRAMS: ExampleProgram[] = [
  {
    id: 'simple_loop',
    name: '1. Loop & Changing Variable',
    filename: 'simple_loop.c',
    description: 'Demonstrates repeated single-steps, changing registers (RAX, RIP), and program stdout.',
    source: `#include <stdio.h>

int main(void) {
    printf("Starting loop execution...\\n");
    for (int i = 0; i < 5; i++) {
        int val = i * 10;
        printf("Iteration %d: val = %d\\n", i, val);
    }
    printf("Loop complete!\\n");
    return 0;
}
`,
  },
  {
    id: 'variables',
    name: '2. Multiple Variables & Arithmetic',
    filename: 'variables.c',
    description: 'Demonstrates multi-register state inspection and register value differences.',
    source: `#include <stdio.h>

int main(void) {
    int a = 42;
    int b = 100;
    int sum = a + b;
    int product = a * b;
    int diff = b - a;

    printf("Sum: %d\\n", sum);
    printf("Product: %d\\n", product);
    printf("Diff: %d\\n", diff);
    return 0;
}
`,
  },
  {
    id: 'branching',
    name: '3. Conditional Branching',
    filename: 'branching.c',
    description: 'Demonstrates conditional jumps (JNE/JE) and control-flow trajectory.',
    source: `#include <stdio.h>

int main(void) {
    int threshold = 25;
    int input = 30;

    if (input > threshold) {
        printf("Branch Taken: input %d exceeds threshold %d\\n", input, threshold);
    } else {
        printf("Branch Not Taken: input %d is within threshold %d\\n", input, threshold);
    }

    return 0;
}
`,
  },
  {
    id: 'functions',
    name: '4. Nested Functions & Stack Frame',
    filename: 'functions.c',
    description: 'Demonstrates function calls (CALL/RET) and stack pointer (RSP/RBP) alterations.',
    source: `#include <stdio.h>

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
    printf("Computation result: %d\\n", res);
    return 0;
}
`,
  },
  {
    id: 'signal_trap',
    name: '5. Safe Signal Interception',
    filename: 'signal_trap.c',
    description: 'Demonstrates controlled signal observation (SIGTRAP/SIGUSR1) via ptrace.',
    source: `#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main(void) {
    printf("Preparing to raise a controlled signal...\\n");
    raise(SIGUSR1);
    printf("Execution after signal handled\\n");
    return 0;
}
`,
  },
  {
    id: 'invalid_syntax',
    name: '6. Invalid Compilation (Diagnostics)',
    filename: 'invalid_syntax.c',
    description: 'Verifies compilation failure reporting with accurate compiler diagnostics.',
    source: `#include <stdio.h>

int main(void) {
    int x = 10
    deliberate_syntax_error ;;
    return 0;
}
`,
  },
];

