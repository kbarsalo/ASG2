/* CREATED 4-24-2014 */

#define _POSIX_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include <sys/resource.h>
#include <time.h>

extern void read_tsc_64(u64_t* t);
extern int atoi(char* str);
extern u64_t sub64(u64_t s, u64_t t);
extern int nice(int n);
extern void exit(void);

/* The main program. Calculates pi as a cpu bound test. */
/* Occationally the program emits the process id and how much work has been done */
/* ARGC is the number of arguments entered when running this program */
/* ARGV is the array of character string arguments entered with this program */
/* returns 0 on exit */
int main(int argc, char *argv[]) {
    /* argument 1 is the number of iterations to run */
    /* argument 2 is the argument to nice() */

    /* PI = 4 * (1/1 - 1/3 + 1/5 - 1/7 + 1/9 - 1/11 ...) */
    int i;
    double sum = 0;
    int denom, numer;
    time_t current;
    time_t start = time(NULL);
    int seconds = 0;
    pid_t process_id = getpid();
    int retval, nice_val, iters;
    u64_t s,e,diff;
    double elapsed;
    unsigned long max;

    if (argc < 3) {
        printf("usage: cpu iterations tickets\n");
        return 1;
    }

    iters = atoi(argv[1]);
    nice_val = atoi(argv[2]);
    retval = nice(nice_val);
    if (retval == -1) {
        printf("Error calling nice()\n");
        return 1;
    }
    printf("Process %d at nice %d\n", process_id, nice_val);

    read_tsc_64(&s);
    for (i = 1; i<iters; ++i) {
        denom = 2 * i - 1;
        numer;
        if (i % 2)
            numer = -1;
        else
            numer = 1;
        sum += ((double)numer / (double)denom);
        current = time(NULL);
        if (current - start > seconds) {
            seconds = current - start;
            printf("Process %d has been running for %d seconds.\n", process_id, seconds);
            /*if (seconds == 10)
                break;*/
        }
    }
    read_tsc_64(&e);
    diff = sub64(e, s);
    max = -1;
    elapsed = (double)diff.hi + (double)(diff.lo/100000) / (double)(max/100000);
    printf("CPU process %d (nice %d) calculated pi as %f at %f time units\n", process_id, nice_val, -4 * sum, elapsed);
    return 0;
}
