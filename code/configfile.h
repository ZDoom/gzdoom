#ifndef __CONFIGFILE_H__
#define __CONFIGFILE_H__

class FConfigFile
{
public:
	FConfigFile ();
	FConfigFile (const char *pathname,
		void (*nosechandler)(const char *pathname, FConfigFile *config, void *userdata)=0, void *userdata=NULL);
	FConfigFile (const FConfigFile &other);
	virtual ~FConfigFile ();

	void ClearConfig ();
	FConfigFile &operator= (const FConfigFile &other);

	bool HaveSections () { return Sections != NULL; }
	bool SetSection (const char *section, bool allowCreate=false);
	bool SetFirstSection ();
	bool SetNextSection ();
	const char *GetCurrentSection () const;
	void ClearCurrentSection ();

	bool NextInSection (const char *&key, const char *&value);
	const char *GetValueForKey (const char *key) const;
	void SetValueForKey (const char *key, const char *value, bool duplicates=false);

	const char *GetPathName () const { return PathName; }
	void ChangePathName (const char *path);

	void LoadConfigFile (void (*nosechandler)(const char *pathname, FConfigFile *config, void *userdata), void *userdata);
	void WriteConfigFile () const;

protected:
	virtual void WriteCommentHeader (FILE *file) const;

	virtual char *ReadLine (char *string, int n, void *file) const;
	bool ReadConfig (void *file);

private:
	struct FConfigEntry
	{
		char *Value;
		FConfigEntry *Next;
		char Key[1];	// + length of key

		void SetValue (const char *val);
	};
	struct FConfigSection
	{
		FConfigEntry *RootEntry;
		FConfigEntry **LastEntryPtr;
		FConfigSection *Next;
		char Name[1];	// + length of name
	};

	FConfigSection *Sections;
	FConfigSection **LastSectionPtr;
	FConfigSection *CurrentSection;
	FConfigEntry *CurrentEntry;
	char *PathName;

	FConfigSection *FindSection (const char *name) const;
	FConfigEntry *FindEntry (FConfigSection *section, const char *key) const;
	FConfigSection *NewConfigSection (const char *name);
	FConfigEntry *NewConfigEntry (FConfigSection *section, const char *key, const char *value);
};

#endif //__CONFIGFILE_H__