#ifndef __S_PLAYLIST_H__
#define __S_PLAYLIST_H__

class FPlayList
{
public:
	FPlayList (const char *path);
	~FPlayList ();

	bool ChangeList (const char *path);

	int GetNumSongs () const;
	int SetPosition (int position);
	int GetPosition () const;
	int Advance ();
	const char *GetSong (int position) const;

private:
	static bool NextLine (FILE *file, char *buffer, int n);

	int Position;
	int NumSongs;
	char **Songs;		// Pointers into SongList
	char *SongList;
};

#endif //__S_PLAYLIST_H__