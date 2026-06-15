#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration - will be defined by the test */
extern int run_test(int argc, char **argv);

int main(int argc, char **argv)
{
    int r = run_test(argc, argv);
    FILE *f = fopen("stb_c89_result.txt", "w");
    if (f) { fprintf(f, "DONE_%d\n", r); fclose(f); }
    return 0;
}
