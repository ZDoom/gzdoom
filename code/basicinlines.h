inline fixed_t FixedMulDiv(fixed_t a,fixed_t b,fixed_t c) { return (fixed_t)((double)a*(double)b/(double)c); }
inline fixed_t MulScale12(int a,int b) { return (fixed_t)(((__int64) a * (__int64) b) >> 12); }
inline fixed_t MulScale6(int a,int b) { return (fixed_t)(((__int64) a * (__int64) b) >> 6); }
inline fixed_t MulScale4(int a,int b) { return (fixed_t)(((__int64) a * (__int64) b) >> 4); }
inline fixed_t DivScale6(int a,int b) { return (fixed_t)(((double)a/(double)b)*64.0); }
inline fixed_t FixedRecip(fixed_t a) { return (fixed_t)(4294967296.0/(double)a); }

#define toint(f)			((int)(f))
#define quickertoint(f)		((int)(f))
