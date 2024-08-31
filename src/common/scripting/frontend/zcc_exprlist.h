// Name				Token used in the code generator
xx(Nil,				TK_None)

xx(ID,				TK_Identifier)
xx(Super,			TK_Super)
xx(Null,			TK_Null)
xx(ConstValue,		TK_Const)
xx(FuncCall,		'(')
xx(ArrayAccess,		TK_Array)
xx(MemberAccess,	'.')
xx(ClassCast,		TK_Class)
xx(FunctionPtrCast,	TK_FunctionType)
xx(TypeRef,			TK_Class)
xx(Vector,			TK_Vector2)

xx(PostInc,			TK_Incr)
xx(PostDec,			TK_Decr)

xx(PreInc,			TK_Incr)
xx(PreDec,			TK_Decr)
xx(Negate,			'-')
xx(AntiNegate,		'+')
xx(BitNot,			'~')
xx(BoolNot,			'!')
xx(SizeOf,			TK_SizeOf)
xx(AlignOf,			TK_AlignOf)

xx(Add,				'+')
xx(Sub,				'-')
xx(Mul,				'*')
xx(Div,				'/')
xx(Mod,				'%')
xx(Pow,				TK_MulMul)
xx(CrossProduct,	TK_Cross)
xx(DotProduct,		TK_Dot)
xx(LeftShift,		TK_LShift)
xx(RightShift,		TK_RShift)
xx(URightShift,		TK_URShift)
xx(Concat,			TK_DotDot)

xx(LT,				'<')
xx(LTEQ,			TK_Leq)
xx(GT,				'>')
xx(GTEQ,			TK_Geq)
xx(LTGTEQ,			TK_LtGtEq)
xx(Is,				TK_Is)

xx(EQEQ,			TK_Eq)
xx(NEQ,				TK_Neq)
xx(APREQ,			TK_ApproxEq)

xx(BitAnd,			'&')
xx(BitOr,			'|')
xx(BitXor,			'^')
xx(BoolAnd,			TK_AndAnd)
xx(BoolOr,			TK_OrOr)

xx(Assign,			'=')
xx(AddAssign,		'+')	// these are what the code generator needs, not what they represent.
xx(SubAssign,		'-')
xx(MulAssign,		'*')
xx(DivAssign,		'/')
xx(ModAssign,		'%')
xx(LshAssign,		TK_LShift)
xx(RshAssign,		TK_RShift)
xx(URshAssign,		TK_URShift)
xx(AndAssign,		'&')
xx(OrAssign,		'|')
xx(XorAssign,		'^')

xx(Scope,			TK_ColonColon)

xx(Trinary,			'?')

xx(Cast,			TK_Coerce)

#undef xx
