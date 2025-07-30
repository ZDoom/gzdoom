/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * ptmeffect.c - Code for converting PTM              / / \  \
 *               effects to IT effects.              | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller. Based on xmeffect.c              \_  /  > /
 * by Julien Cugniere.                                  | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */



#include <stdlib.h>
#include <string.h>

#include "dumb.h"
#include "internal/it.h"

void _dumb_it_ptm_convert_effect(int effect, int value, IT_ENTRY *entry)
{
	if (effect >= PTM_N_EFFECTS)
		return;

	/* Linearisation of the effect number... */
	if (effect == PTM_E) {
		effect = PTM_EBASE + HIGH(value);
		value = LOW(value);
	}

	/* convert effect */
	entry->mask |= IT_ENTRY_EFFECT;
	switch (effect) {

		case PTM_APPREGIO:           effect = IT_ARPEGGIO;           break;
		case PTM_PORTAMENTO_UP:      effect = IT_PORTAMENTO_UP;      break;
		case PTM_PORTAMENTO_DOWN:    effect = IT_PORTAMENTO_DOWN;    break;
		case PTM_TONE_PORTAMENTO:    effect = IT_TONE_PORTAMENTO;    break;
		case PTM_VIBRATO:            effect = IT_VIBRATO;            break;
		case PTM_VOLSLIDE_TONEPORTA: effect = IT_VOLSLIDE_TONEPORTA; break;
		case PTM_VOLSLIDE_VIBRATO:   effect = IT_VOLSLIDE_VIBRATO;   break;
		case PTM_TREMOLO:            effect = IT_TREMOLO;            break;
		case PTM_SAMPLE_OFFSET:      effect = IT_SET_SAMPLE_OFFSET;  break;
		case PTM_VOLUME_SLIDE:       effect = IT_VOLUME_SLIDE;       break;
		case PTM_POSITION_JUMP:      effect = IT_JUMP_TO_ORDER;      break;
		case PTM_SET_CHANNEL_VOLUME: effect = IT_SET_CHANNEL_VOLUME; break;
		case PTM_PATTERN_BREAK:      effect = IT_BREAK_TO_ROW;       break;
		case PTM_SET_GLOBAL_VOLUME:  effect = IT_SET_GLOBAL_VOLUME;  break;
		case PTM_RETRIGGER:          effect = IT_RETRIGGER_NOTE;     break;
		case PTM_FINE_VIBRATO:       effect = IT_FINE_VIBRATO;       break;

		/* TODO properly */
		case PTM_NOTE_SLIDE_UP:          effect = IT_PTM_NOTE_SLIDE_UP;          break;
		case PTM_NOTE_SLIDE_DOWN:        effect = IT_PTM_NOTE_SLIDE_DOWN;        break;
		case PTM_NOTE_SLIDE_UP_RETRIG:   effect = IT_PTM_NOTE_SLIDE_UP_RETRIG;   break;
		case PTM_NOTE_SLIDE_DOWN_RETRIG: effect = IT_PTM_NOTE_SLIDE_DOWN_RETRIG; break;

		case PTM_SET_TEMPO_BPM:
			effect = (value < 0x20) ? (IT_SET_SPEED) : (IT_SET_SONG_TEMPO);
			break;

		case PTM_EBASE+PTM_E_SET_FINETUNE:          effect = SBASE+IT_S_FINETUNE;              break; /** TODO */
		case PTM_EBASE+PTM_E_SET_LOOP:              effect = SBASE+IT_S_PATTERN_LOOP;          break;
		case PTM_EBASE+PTM_E_NOTE_CUT:              effect = SBASE+IT_S_DELAYED_NOTE_CUT;      break;
		case PTM_EBASE+PTM_E_NOTE_DELAY:            effect = SBASE+IT_S_NOTE_DELAY;            break;
		case PTM_EBASE+PTM_E_PATTERN_DELAY:         effect = SBASE+IT_S_PATTERN_DELAY;         break;
		case PTM_EBASE+PTM_E_SET_PANNING:           effect = SBASE+IT_S_SET_PAN;               break;

		case PTM_EBASE+PTM_E_FINE_VOLSLIDE_UP:
			effect = IT_VOLUME_SLIDE;
			value = EFFECT_VALUE(value, 0xF);
			break;

		case PTM_EBASE + PTM_E_FINE_VOLSLIDE_DOWN:
			effect = IT_VOLUME_SLIDE;
			value = EFFECT_VALUE(0xF, value);
			break;

		case PTM_EBASE + PTM_E_FINE_PORTA_UP:
			effect = IT_PORTAMENTO_UP;
			value = EFFECT_VALUE(0xF, value);
			break;

		case PTM_EBASE + PTM_E_FINE_PORTA_DOWN:
			effect = IT_PORTAMENTO_DOWN;
			value = EFFECT_VALUE(0xF, value);
			break;

		case PTM_EBASE + PTM_E_RETRIG_NOTE:
			effect = IT_XM_RETRIGGER_NOTE;
			value = EFFECT_VALUE(0, value);
			break;

		case PTM_EBASE + PTM_E_SET_VIBRATO_CONTROL:
			effect = SBASE+IT_S_SET_VIBRATO_WAVEFORM;
			value &= ~4; /** TODO: value&4 -> don't retrig wave */
			break;

		case PTM_EBASE + PTM_E_SET_TREMOLO_CONTROL:
			effect = SBASE+IT_S_SET_TREMOLO_WAVEFORM;
			value &= ~4; /** TODO: value&4 -> don't retrig wave */
			break;

		default:
			/* user effect (often used in demos for synchronisation) */
			entry->mask &= ~IT_ENTRY_EFFECT;
	}

	/* Inverse linearisation... */
	if (effect >= SBASE && effect < SBASE+16) {
		value = EFFECT_VALUE(effect-SBASE, value);
		effect = IT_S;
	}

	entry->effect = effect;
	entry->effectvalue = value;
}
