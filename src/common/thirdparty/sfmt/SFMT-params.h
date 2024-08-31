#ifndef SFMT_PARAMS_H
#define SFMT_PARAMS_H

/*----------------------
  the parameters of SFMT
  following definitions are in paramsXXXX.h file.
  ----------------------*/
/** the pick up position of the array.
#define POS1 122 
*/

/** the parameter of shift left as four 32-bit registers.
#define SL1 18
 */

/** the parameter of shift left as one 128-bit register. 
 * The 128-bit integer is shifted by (SL2 * 8) bits. 
#define SL2 1 
*/

/** the parameter of shift right as four 32-bit registers.
#define SR1 11
*/

/** the parameter of shift right as one 128-bit register. 
 * The 128-bit integer is shifted by (SL2 * 8) bits. 
#define SR2 1 
*/

/** A bitmask, used in the recursion.  These parameters are introduced
 * to break symmetry of SIMD.
#define MSK1 0xdfffffefU
#define MSK2 0xddfecb7fU
#define MSK3 0xbffaffffU
#define MSK4 0xbffffff6U 
*/

/** These definitions are part of a 128-bit period certification vector.
#define PARITY1	0x00000001U
#define PARITY2	0x00000000U
#define PARITY3	0x00000000U
#define PARITY4	0xc98e126aU
*/

#if MEXP == 607
  #include "SFMT-params607.h"
#elif MEXP == 1279
  #include "SFMT-params1279.h"
#elif MEXP == 2281
  #include "SFMT-params2281.h"
#elif MEXP == 4253
  #include "SFMT-params4253.h"
#elif MEXP == 11213
  #include "SFMT-params11213.h"
#elif MEXP == 19937
  #include "SFMT-params19937.h"
#elif MEXP == 44497
  #include "SFMT-params44497.h"
#elif MEXP == 86243
  #include "SFMT-params86243.h"
#elif MEXP == 132049
  #include "SFMT-params132049.h"
#elif MEXP == 216091
  #include "SFMT-params216091.h"
#else
  #error "MEXP is not valid."
  #undef MEXP
#endif

#endif /* SFMT_PARAMS_H */
