#ifndef __crankshaftrandomdoth__
#define __crankshaftrandomdoth__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************
 *
 * Random number generator of our own. It's bad
 * but we can count on it to be the same between
 * all platforms. So when we run tests with
 * random numbers it'll work the same everywhere
 *
 *********************************************/
struct CS_LCG_rand_state {
    int seed;
};

extern struct CS_LCG_rand_state globalState;

int CS_LCG_rand( struct CS_LCG_rand_state *state );
void CS_LCG_rand_init( struct CS_LCG_rand_state *state, int seed );

#define CS_rand() CS_LCG_rand(&globalState)
#define CS_srand(X) (globalState.seed=(X))

#ifdef __cplusplus
}
#endif
#endif
