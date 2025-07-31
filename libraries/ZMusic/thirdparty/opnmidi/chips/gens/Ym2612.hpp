/***************************************************************************
 * libgens: Gens Emulation Library.                                        *
 * Ym2612.hpp: Yamaha YM2612 FM synthesis chip emulator.                   *
 *                                                                         *
 * Copyright (c) 1999-2002 by Stéphane Dallongeville                       *
 * Copyright (c) 2003-2004 by Stéphane Akhoun                              *
 * Copyright (c) 2008-2015 by David Korth                                  *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#ifndef __LIBGENS_SOUND_YM2612_HPP__
#define __LIBGENS_SOUND_YM2612_HPP__

#include <stdint.h>

namespace LibGens {

class Ym2612Private;
class Ym2612
{
	public:
		Ym2612();
		Ym2612(int clock, int rate);
		~Ym2612();

	protected:
		friend class Ym2612Private;
		Ym2612Private *const d;
	private:
		// Q_DISABLE_COPY() equivalent.
		// TODO: Add LibGens-specific version of Q_DISABLE_COPY().
		Ym2612(const Ym2612 &);
		Ym2612 &operator=(const Ym2612 &);

	public:
		int reInit(int clock, int rate);
		void reset(void);

		uint8_t read(void) const;
		int write(unsigned int address, uint8_t data);
		void write_pan(int channel, int data);
		void update(int32_t *bufL, int32_t *bufR, int length);

		// Properties.
		// TODO: Read-only for now.
		bool enabled(void) const { return m_enabled; }
		bool dacEnabled(void) const { return m_dacEnabled; }
		bool improved(void) const { return m_improved; }

		/** Gens-specific code. **/
		void updateDacAndTimers(int32_t *bufL, int32_t *bufR, int length);
		void specialUpdate(void);
		int getReg(int regID) const;

		// YM write length.
		inline void addWriteLen(int len)
			{ m_writeLen += len; }
		inline void clearWriteLen(void)
			{ m_writeLen = 0; }

		// Reset buffer pointers.
		void resetBufferPtrs(int32_t *bufPtrL, int32_t *bufPtrR);

	protected:
		// PSG write length. (for audio output)
		int m_writeLen;
		bool m_enabled;		// YM2612 Enabled
		bool m_dacEnabled;	// DAC Enabled
		bool m_improved;	// YM2612 Improved

		// YM buffer pointers.
		// TODO: Figure out how to get rid of these!
		int32_t *m_bufPtrL;
		int32_t *m_bufPtrR;
};

/* end */

}

#endif /* __LIBGENS_SOUND_YM2612_HPP__ */

