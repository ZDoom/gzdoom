// Data reader interface for uniform access

// File_Extractor 0.4.0
#ifndef DATA_READER_H
#define DATA_READER_H

#include "blargg_common.h"

// Supports reading and finding out how many bytes are remaining
class Data_Reader {
public:
	virtual ~Data_Reader() { }
	
	static const char eof_error []; // returned by read() when request goes beyond end
	
	// Read at most count bytes and return number actually read, or <= 0 if error
	virtual long read_avail( void*, long n ) = 0;
	
	// Read exactly count bytes and return error if they couldn't be read
	virtual blargg_err_t read( void*, long count );
	
	// Number of bytes remaining until end of file
	virtual long remain() const = 0;
	
	// Read and discard count bytes
	virtual blargg_err_t skip( long count );
	
public:
	Data_Reader() { }
	typedef blargg_err_t error_t; // deprecated
private:
	// noncopyable
	Data_Reader( const Data_Reader& );
	Data_Reader& operator = ( const Data_Reader& );
};

// Supports seeking in addition to Data_Reader operations
class File_Reader : public Data_Reader {
public:
	// Size of file
	virtual long size() const = 0;
	
	// Current position in file
	virtual long tell() const = 0;
	
	// Go to new position
	virtual blargg_err_t seek( long ) = 0;
	
	long remain() const;
	blargg_err_t skip( long n );
};

// Disk file reader
class Std_File_Reader : public File_Reader {
public:
	blargg_err_t open( const char* path );
	void close();
	
public:
	Std_File_Reader();
	~Std_File_Reader();
	long size() const;
	blargg_err_t read( void*, long );
	long read_avail( void*, long );
	long tell() const;
	blargg_err_t seek( long );
private:
	void* file_;
};

// Treats range of memory as a file
class Mem_File_Reader : public File_Reader {
public:
	Mem_File_Reader( const void*, long size );
	
public:
	long size() const;
	long read_avail( void*, long );
	long tell() const;
	blargg_err_t seek( long );
private:
	const char* const begin;
	const long size_;
	long pos;
};

// Makes it look like there are only count bytes remaining
class Subset_Reader : public Data_Reader {
public:
	Subset_Reader( Data_Reader*, long count );

public:
	long remain() const;
	long read_avail( void*, long );
private:
	Data_Reader* in;
	long remain_;
};

// Joins already-read header and remaining data into original file (to avoid seeking)
class Remaining_Reader : public Data_Reader {
public:
	Remaining_Reader( void const* header, long size, Data_Reader* );

public:
	long remain() const;
	long read_avail( void*, long );
	blargg_err_t read( void*, long );
private:
	char const* header;
	char const* header_end;
	Data_Reader* in;
	long read_first( void* out, long count );
};

// Invokes callback function to read data. Size of data must be specified in advance.
class Callback_Reader : public Data_Reader {
public:
	typedef const char* (*callback_t)( void* data, void* out, int count );
	Callback_Reader( callback_t, long size, void* data = 0 );
public:
	long read_avail( void*, long );
	blargg_err_t read( void*, long );
	long remain() const;
private:
	callback_t const callback;
	void* const data;
	long remain_;
};

#ifdef HAVE_ZLIB_H
// Gzip compressed file reader
class Gzip_File_Reader : public File_Reader {
public:
	blargg_err_t open( const char* path );
	void close();
	
public:
	Gzip_File_Reader();
	~Gzip_File_Reader();
	long size() const;
	long read_avail( void*, long );
	long tell() const;
	blargg_err_t seek( long );
private:
	void* file_;
	long size_;
};
#endif

#endif
