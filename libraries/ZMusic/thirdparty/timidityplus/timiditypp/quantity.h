/* 
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    quantity.h
    
   	by Kentaro Sato	<kentaro@ranvis.com>
*/

#ifndef ___QUANTITY_H_
#define ___QUANTITY_H_

#include <stdint.h>

namespace TimidityPlus
{


#define QUANTITY_UNIT_TYPE(u)		QUANTITY_OF_##u, QUANTITY_UNIT_NAME(u##_NUM)
#define QUANTITY_UNIT_NAME(name)	QUANTITY_UNIT_##name
enum quantity_units {
	QUANTITY_UNIT_TYPE(UNDEFINED),		/* type only */
	QUANTITY_UNIT_TYPE(DIRECT_INT),		/* internal use */
	QUANTITY_UNIT_TYPE(DIRECT_FLOAT),	/* internal use */
	QUANTITY_UNIT_TYPE(TREMOLO_SWEEP),			/* int */
		QUANTITY_UNIT_NAME(TREMOLO_SWEEP_MS),	/* int */
	QUANTITY_UNIT_TYPE(TREMOLO_RATE),			/* int */
		QUANTITY_UNIT_NAME(TREMOLO_RATE_MS),	/* int */
		QUANTITY_UNIT_NAME(TREMOLO_RATE_HZ),	/* float */
	QUANTITY_UNIT_TYPE(VIBRATO_SWEEP),			/* int */
		QUANTITY_UNIT_NAME(VIBRATO_SWEEP_MS),	/* int */
	QUANTITY_UNIT_TYPE(VIBRATO_RATE),			/* int */
		QUANTITY_UNIT_NAME(VIBRATO_RATE_MS),	/* int */
		QUANTITY_UNIT_NAME(VIBRATO_RATE_HZ),	/* float */
};
#undef QUANTITY_UNIT_TYPE
#define QUANTITY_UNIT_TYPE(u)		QUANTITY_OF_##u

#define INIT_QUANTITY(q)		(q).type = QUANTITY_UNIT_TYPE(UNDEFINED)
#define IS_QUANTITY_DEFINED(q)	((q).type != QUANTITY_UNIT_TYPE(UNDEFINED))

typedef struct Quantity_ {
	uint16_t			type, unit;
	union {
		int32_t			i;
		double			f;
	} value;
} Quantity;

extern const char *string_to_quantity(const char *string, Quantity *quantity, uint16_t type);
extern int32_t quantity_to_int(const Quantity *quantity, int32_t param);
extern double quantity_to_float(const Quantity *quantity, int32_t param);

}
#endif /* ___QUANTITY_H_ */
