#include <stdio.h>
#include <zmusic.h>

int main()
{
	int count = 0;
	const ZMusicMidiOutDevice *devices = ZMusic_GetMidiDevices(&count);
	
	for (int i = 0; i < count; ++i)
	{
		const ZMusicMidiOutDevice &d = devices[i];
		const char *tech = "<Unknown>";
		switch (d.Technology)
		{
		case MIDIDEV_MIDIPORT:  tech = "MIDIPORT";  break;
		case MIDIDEV_SYNTH:     tech = "SYNTH";     break;
		case MIDIDEV_SQSYNTH:   tech = "SQSYNTH";   break;
		case MIDIDEV_FMSYNTH:   tech = "FMSYNTH";   break;
		case MIDIDEV_MAPPER:    tech = "MAPPER";    break;
		case MIDIDEV_WAVETABLE: tech = "WAVETABLE"; break;
		case MIDIDEV_SWSYNTH:   tech = "SWSYNTH";   break;
		}
		printf("[%i] %i. %s: %s\n", i, d.ID, d.Name, tech);
	}
	
	return 0;
}
