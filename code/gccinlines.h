// killough 5/10/98: In djgpp, use inlined assembly for performance

__inline__ static fixed_t FixedMul(fixed_t a, fixed_t b)
{
  fixed_t result;

  asm("\timull %2\n"
      "\tshrdl $16,%%edx,%0"
      : "=a,a" (result)           // eax is always the result
      : "0,0" (a),                 // eax is also first operand
        "m,r" (b)                  // second operand can be mem or reg
      : "%edx", "%cc"              // edx and condition codes clobbered
      );

  return result;
}

__inline__ static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
  fixed_t result;

  if (abs(a) >> 14 >= abs(b))
    return (a^b)<0 ? MININT : MAXINT;

  asm(" movl %0, %%edx ;"
      " sall $16,%%eax ;"
      " sarl $16,%%edx ;"
      " idivl %2 ;"
      : "=a,a" (result)     // eax is always the result
      : "0,0" (a),          // eax is also the first operand
        "m,r" (b)           // second operand can be mem or reg (not imm)
      : "%edx", "%cc"       // edx and condition codes are clobbered
      );

  return result;
}
