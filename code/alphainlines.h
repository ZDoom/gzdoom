inline fixed_t FixedMul (fixed_t a, fixed_t b)
{
    return (fixed_t)(((long)a * (long)b) >> 16);
}

inline fixed_t FixedMulDiv (fixed_t a, fixed_t b, fixed_t c)
{
	return (fixed_t)((long)a * (long)b / (long)c);
}

inline fixed_t FixedDiv (fixed_t a, fixed_t b)
{
    if (abs(a) >> 15 >= abs(b))
	    return (a^b)<0 ? MININT : MAXINT;
	return (fixed_t)((((long)a) << 16) / b);
}
