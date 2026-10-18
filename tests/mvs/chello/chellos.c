/* chellos.c -- chello.c with a 16 KB C stack in place of the default
 * 256 KB (__stklen, LIBC/370's @@STKLEN), for #177. */
#include <stdio.h>

unsigned __stklen = 16 * 1024;

int main(void)
{
    puts("HELLO");
    return 0;
}
