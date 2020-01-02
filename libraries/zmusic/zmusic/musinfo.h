#pragma once

#include <string>
#include <mutex>
#include "mididefs.h"
#include "zmusic/zmusic_internal.h"

// The base music class. Everything is derived from this --------------------

class MusInfo
{
public:
	MusInfo() = default;
	virtual ~MusInfo() {}
	virtual void MusicVolumeChanged() {}		// snd_musicvolume changed
	virtual void Play (bool looping, int subsong) = 0;
	virtual void Pause () = 0;
	virtual void Resume () = 0;
	virtual void Stop () = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsMIDI() const { return false; }
	virtual bool IsValid () const = 0;
	virtual bool SetPosition(unsigned int ms) { return false;  }
	virtual bool SetSubsong (int subsong) { return false; }
	virtual void Update() {}
	virtual int GetDeviceType() const { return MDEV_DEFAULT; }	// MDEV_DEFAULT stands in for anything that cannot change playback parameters which needs a restart.
	virtual std::string GetStats() { return "No stats available for this song"; }
	virtual MusInfo* GetWaveDumper(const char* filename, int rate) { return nullptr;  }
	virtual void ChangeSettingInt(const char* setting, int value) {}			// FluidSynth settings
	virtual void ChangeSettingNum(const char* setting, double value) {}		// "
	virtual void ChangeSettingString(const char* setting, const char* value) {}	// "
	virtual bool ServiceStream(void *buff, int len) { return false;  }
	virtual SoundStreamInfo GetStreamInfo() const { return { 0,0,0 }; }

	enum EState
	{
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
	} m_Status = STATE_Stopped;
	bool m_Looping = false;
	std::mutex CritSec;
};
