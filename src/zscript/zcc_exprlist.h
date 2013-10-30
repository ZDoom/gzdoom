// Name				n-ary
xx(Nil,				0)

xx(ID,				0)
xx(Super,			0)
xx(Self,			0)
xx(ConstValue,		0)
xx(FuncCall,		0)
xx(ArrayAccess,		0)
xx(MemberAccess,	0)
xx(TypeRef,			0)

xx(PostInc,			1)
xx(PostDec,			1)

xx(PreInc,			1)
xx(PreDec,			1)
xx(Negate,			1)
xx(AntiNegate,		1)
xx(BitNot,			1)
xx(BoolNot,			1)
xx(SizeOf,			1)
xx(AlignOf,			1)

xx(Add,				2)
xx(Sub,				2)
xx(Mul,				2)
xx(Div,				2)
xx(Mod,				2)
xx(Pow,				2)
xx(CrossProduct,	2)
xx(DotProduct,		2)
xx(LeftShift,		2)
xx(RightShift,		2)
xx(Concat,			2)

xx(LT,				2)
xx(LTEQ,			2)
xx(LTGTEQ,			2)
xx(Is,				2)

xx(EQEQ,			2)
xx(APREQ,			2)

xx(BitAnd,			2)
xx(BitOr,			2)
xx(BitXor,			2)
xx(BoolAnd,			2)
xx(BoolOr,			2)

xx(Scope,			0)

xx(Trinary,			2)

xx(Cast,			1)

#undef xx
