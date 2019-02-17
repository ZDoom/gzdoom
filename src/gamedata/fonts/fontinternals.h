#pragma once

#include <stdint.h>
#include "tarray.h"

// This structure is used by BuildTranslations() to hold color information.
struct TranslationParm
{
	short RangeStart;	// First level for this range
	short RangeEnd;		// Last level for this range
	uint8_t Start[3];		// Start color for this range
	uint8_t End[3];		// End color for this range
};


extern TArray<TranslationParm> TranslationParms[2];

void RecordTextureColors (FImageSource *pic, uint8_t *usedcolors);
