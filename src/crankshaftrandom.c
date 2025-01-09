#include <stdlib.h>
#include <stdio.h>

#include "crankshaft/alloc.h"
#include "crankshaft/logger.h"
#include "crankshaft/random.h"

struct CS_LCG_rand_state globalState = { 78234 };

int CS_LCG_rand( struct CS_LCG_rand_state *state ) {
    state->seed = 1103515245 * state->seed + 12345;
    return state->seed;
}

void CS_LCG_rand_init( struct CS_LCG_rand_state *state, int seed ) {
    state->seed = seed;
}

