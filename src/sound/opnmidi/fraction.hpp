#ifndef bqw_fraction_h
#define bqw_fraction_h

#include <cmath>
#include <limits>

/* Fraction number handling.
 * Copyright (C) 1992,2001 Bisqwit (http://iki.fi/bisqwit/)
 */

template<typename inttype=int>
class fraction
{
    inttype num1, num2;
    typedef fraction<inttype> self;
    void Optim();

  #if 1
    inline void Debug(char, const self &) { }
  #else
    inline void Debug(char op, const self &b)
    {
        cerr << nom() << '/' << denom() << ' ' << op
             << ' ' << b.nom() << '/' << denom()
             << ":\n";
    }
  #endif
public:
    void set(inttype n, inttype d) { num1=n; num2=d; Optim(); }

    fraction() : num1(0), num2(1) { }
    fraction(inttype value) : num1(value), num2(1) { }
    fraction(inttype n, inttype d) : num1(n), num2(d) { }
    fraction(int value) : num1(value), num2(1) { }
    template<typename floattype>
    explicit fraction(const floattype value) { operator= (value); }
    inline double value() const {return nom() / (double)denom(); }
    inline long double valuel() const {return nom() / (long double)denom(); }
    self &operator+= (const inttype &value) { num1+=value*denom(); Optim(); return *this; }
    self &operator-= (const inttype &value) { num1-=value*denom(); Optim(); return *this; }
    self &operator*= (const inttype &value) { num1*=value; Optim(); return *this; }
    self &operator/= (const inttype &value) { num2*=value; Optim(); return *this; }
    self &operator+= (const self &b);
    self &operator-= (const self &b);
    self &operator*= (const self &b) { Debug('*',b);num1*=b.nom(); num2*=b.denom(); Optim(); return *this; }
    self &operator/= (const self &b) { Debug('/',b);num1*=b.denom(); num2*=b.nom(); Optim(); return *this; }
    self operator- () const { return self(-num1, num2); }

#define fraction_blah_func(op1, op2) \
      self operator op1 (const self &b) const { self tmp(*this); tmp op2 b; return tmp; }

    fraction_blah_func( +, += )
    fraction_blah_func( -, -= )
    fraction_blah_func( /, /= )
    fraction_blah_func( *, *= )

#undef fraction_blah_func
#define fraction_blah_func(op) \
      bool operator op(const self &b) const { return value() op b.value(); } \
      bool operator op(inttype b) const { return value() op b; }

    fraction_blah_func( < )
    fraction_blah_func( > )
    fraction_blah_func( <= )
    fraction_blah_func( >= )

#undef fraction_blah_func

    const inttype &nom() const { return num1; }
    const inttype &denom() const { return num2; }
    inline bool operator == (inttype b) const { return denom() == 1 && nom() == b; }
    inline bool operator != (inttype b) const { return denom() != 1 || nom() != b; }
    inline bool operator == (const self &b) const { return denom()==b.denom() && nom()==b.nom(); }
    inline bool operator != (const self &b) const { return denom()!=b.denom() || nom()!=b.nom(); }
    //operator bool () const { return nom() != 0; }
    inline bool negative() const { return (nom() < 0) ^ (denom() < 0); }

    self &operator= (const inttype value) { num2=1; num1=value; return *this; }
    //self &operator= (int value) { num2=1; num1=value; return *this; }

    self &operator= (double orig) { return *this = (long double)orig; }
    self &operator= (long double orig);
};

#ifdef _MSC_VER
#pragma warning(disable:4146)
#endif

template<typename inttype>
void fraction<inttype>::Optim()
{
    /* Euclidean algorithm */
    inttype n1, n2, nn1, nn2;

    nn1 = std::numeric_limits<inttype>::is_signed ? (num1 >= 0 ? num1 : -num1) : num1;
    nn2 = std::numeric_limits<inttype>::is_signed ? (num2 >= 0 ? num2 : -num2) : num2;

    if(nn1 < nn2)
        n1 = num1, n2 = num2;
    else
        n1 = num2, n2 = num1;

    if(!num1) { num2 = 1; return; }
    for(;;)
    {
        //fprintf(stderr, "%d/%d: n1=%d,n2=%d\n", nom(),denom(),n1,n2);
        inttype tmp = n2 % n1;
        if(!tmp)break;
        n2 = n1;
        n1 = tmp;
    }
    num1 /= n1;
    num2 /= n1;
    //fprintf(stderr, "result: %d/%d\n\n", nom(), denom());
}

#ifdef _MSC_VER
#pragma warning(default:4146)
#endif

template<typename inttype>
inline const fraction<inttype> abs(const fraction<inttype> &f)
{
    return fraction<inttype>(abs(f.nom()), abs(f.denom()));
}

#define fraction_blah_func(op) \
  template<typename inttype> \
  fraction<inttype> operator op \
    (const inttype bla, const fraction<inttype> &b) \
      { return fraction<inttype> (bla) op b; }
fraction_blah_func( + )
fraction_blah_func( - )
fraction_blah_func( * )
fraction_blah_func( / )
#undef fraction_blah_func

#define fraction_blah_func(op1, op2) \
  template<typename inttype> \
  fraction<inttype> &fraction<inttype>::operator op2 (const fraction<inttype> &b) \
    { \
        inttype newnom = nom()*b.denom() op1 denom()*b.nom(); \
        num2 *= b.denom(); \
        num1 = newnom; \
        Optim(); \
        return *this; \
    }
fraction_blah_func( +, += )
fraction_blah_func( -, -= )
#undef fraction_blah_func

template<typename inttype>
fraction<inttype> &fraction<inttype>::operator= (long double orig)
{
    if(orig == 0.0)
    {
        set(0, 0);
        return *this;
    }

    inttype cf[25];
    for(int maxdepth=1; maxdepth<25; ++maxdepth)
    {
        inttype u,v;
        long double virhe, a=orig;
        int i, viim;

        for(i = 0; i < maxdepth; ++i)
        {
            cf[i] = (inttype)a;
            if(cf[i]-1 > cf[i])break;
            a = 1.0 / (a - cf[i]);
        }

        for(viim=i-1; i < maxdepth; ++i)
            cf[i] = 0;

        u = cf[viim];
        v = 1;
        for(i = viim-1; i >= 0; --i)
        {
            inttype w = cf[i] * u + v;
            v = u;
            u = w;
        }

        virhe = (orig - (u / (long double)v)) / orig;

        set(u, v);
        //if(verbose > 4)
        //    cerr << "Guess: " << *this << " - error = " << virhe*100 << "%\n";

        if(virhe < 1e-8 && virhe > -1e-8)break;
    }

    //if(verbose > 4)
    //{
    //    cerr << "Fraction=" << orig << ": " << *this << endl;
    //}

    return *this;
}


/*
template<typename inttype>
ostream &operator << (ostream &dest, const fraction<inttype> &m)
{
    if(m.denom() == (inttype)1) return dest << m.nom();
    return dest << m.nom() << '/' << m.denom();
}
*/

#endif
