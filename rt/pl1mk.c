/* pl1mk.c -- the constants more than one family of the mathematical
 * functions uses (rt/pl1mth.h); a member of its own, so that SQRT, which
 * needs none, does not link them. */
#include "pl1mth.h"

const struct pl1mk pl1mk = {
    .expmax = {{0x42AEAC4F, 0x97F2883C}},   /* 174.67308950110623 */
    .expmin = {{0xC2B437E0, 0x57B116B8}},   /* -180.21826694558578 */
    .x_ln2a = {0, 0, 0xB17217F7D1CF7800ULL},
    .x_ln2b = {0, -55, 0xD5E4F1D9CC01F97BULL},
    .x_one = {0, 1, 0x8000000000000000ULL},
    .x_two = {0, 2, 0x8000000000000000ULL},
    .x_third = {0, -1, 0xAAAAAAAAAAAAAAABULL},
    .x_sixth = {0, -2, 0xAAAAAAAAAAAAAAABULL},
};
