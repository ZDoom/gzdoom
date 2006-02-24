enum
{
	EX_Return		= 0x00,
	EX_EndFuncParms	= 0x01,
	EX_ByteConst	= 0x02,
	EX_WordConst	= 0x03,
	EX_DWordConst	= 0x04,
	EX_FixedConst	= 0x05,		// To become FloatConst whenever I move to floats
	EX_StringConst	= 0x06,

	EX_Extended1	= 0xF1,
	EX_Extended2	= 0xF2,
	EX_Extended3	= 0xF3,
	EX_Extended4	= 0xF4,
	EX_Extended5	= 0xF5,
	EX_Extended6	= 0xF6,
	EX_Extended7	= 0xF7,
	EX_Extended8	= 0xF8,
	EX_Extended9	= 0xF9,
	EX_ExtendedA	= 0xFA,
	EX_ExtendedB	= 0xFB,
	EX_ExtendedC	= 0xFC,
	EX_ExtendedD	= 0xFD,
	EX_ExtendedE	= 0xFE,
	EX_ExtendedF	= 0xFF
};