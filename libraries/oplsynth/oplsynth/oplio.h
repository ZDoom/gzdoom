#pragma once


enum
{
	// Operator registers (21 of each):
	OPL_REGS_TREMOLO          = 0x20,
	OPL_REGS_LEVEL            = 0x40,
	OPL_REGS_ATTACK           = 0x60,
	OPL_REGS_SUSTAIN          = 0x80,
	OPL_REGS_WAVEFORM         = 0xE0,

	// Voice registers (9 of each):
	OPL_REGS_FREQ_1           = 0xA0,
	OPL_REGS_FREQ_2           = 0xB0,
	OPL_REGS_FEEDBACK         = 0xC0,
};

enum
{
	OPL_REG_WAVEFORM_ENABLE   = 0x01,
	OPL_REG_TIMER1            = 0x02,
	OPL_REG_TIMER2            = 0x03,
	OPL_REG_TIMER_CTRL        = 0x04,
	OPL_REG_FM_MODE           = 0x08,
	OPL_REG_PERCUSSION_MODE   = 0xBD,
	
	OPL_REG_OPL3_ENABLE       = 0x105,
	OPL_REG_4OPMODE_ENABLE    = 0x104,
	
};

enum
{
	NO_VOLUME = 0x3f,
	MAX_ATTACK_DECAY = 0xff,
	NO_SUSTAIN_MAX_RELEASE = 0xf,
	WAVEFORM_ENABLED = 0x20
};

enum
{
	OPL_NUM_VOICES	= 9,
	OPL3_NUM_VOICES	= 18,
	MAXOPL2CHIPS	= 8,
	NUM_VOICES		= (OPL_NUM_VOICES * MAXOPL2CHIPS),

	NUM_CHANNELS	= 16,
	CHAN_PERCUSSION	= 15,

	VIBRATO_THRESHOLD = 40,
	MIN_SUSTAIN = 0x40,
	HIGHEST_NOTE = 127,

};

struct GenMidiVoice;
struct genmidi_op_t;

struct OPLio 
{
	virtual ~OPLio();

	void WriteOperator(uint32_t regbase, uint32_t channel, int index, uint8_t data2);
	void LoadOperatorData(uint32_t channel, int op_index, genmidi_op_t *op_data, bool maxlevel, bool vibrato);
	void WriteValue(uint32_t regbase, uint32_t channel, uint8_t value);
	void WriteFrequency(uint32_t channel, uint32_t freq, uint32_t octave, uint32_t keyon);
	void WriteVolume(uint32_t channel, GenMidiVoice *voice, uint32_t v1, uint32_t v2, uint32_t v3);
	void WritePan(uint32_t channel, GenMidiVoice *voice, int pan);
	void WriteTremolo(uint32_t channel, GenMidiVoice *voice, bool vibrato);
	void WriteInstrument(uint32_t channel, GenMidiVoice *voice, bool vibrato);
	void WriteInitState(bool opl3);
	void MuteChannel(uint32_t chan);
	void StopPlayback();

	virtual int	 Init(int core, uint32_t numchips, bool stereo, bool initopl3);
	virtual void Reset();
	virtual void WriteRegister(int which, uint32_t reg, uint8_t data);
	virtual void SetClockRate(double samples_per_tick);
	virtual void WriteDelay(int ticks);

	class OPLEmul *chips[OPL_NUM_VOICES];
	uint32_t NumChannels;
	uint32_t NumChips;
	bool IsOPL3;
};

struct OPLChannel
{
	uint32_t Instrument;
	uint8_t Volume;
	uint8_t Panning;
	int8_t Pitch;
	uint8_t Sustain;
	bool Vibrato;
	uint8_t Expression;
	uint16_t PitchSensitivity;
	uint16_t RPN;
};
