#ifndef __TEMPFILES_H__
#define __TEMPFILES_H__

#ifdef _MSC_VER
#pragma once
#endif

// Returns a file name suitable for use as a temp file.
// If you create a file with this name (and presumably you
// will), it will be deleted automatically by this class's
// destructor.

class FTempFileName
{
public:
	FTempFileName (const char *prefix=NULL);
	~FTempFileName ();

	operator const char * const () { return Name; }
	const char * GetName () const { return Name; }

private:
	char *Name;
};

#endif //__TEMPFILES_H__
