/*
 Author: Marcus Boerger <helly@users.sourceforge.net>
*/

/* $Id: stream_lc.h 767 2007-06-26 15:21:10Z helly $ */

#ifndef _stream_lc_h
#define _stream_lc_h

#include <iosfwd>
#include <fstream>
#include <assert.h>
#include <stdio.h>

namespace re2c
{

template<class _E, class _Tr = std::char_traits<_E> >
class basic_null_streambuf
	: public std::basic_streambuf<_E, _Tr>
{
public:
	basic_null_streambuf()
		: std::basic_streambuf<_E, _Tr>()
	{
	}	
};

typedef basic_null_streambuf<char> null_streambuf;

template<class _E, class _Tr = std::char_traits<_E> >
class basic_null_stream
	: public std::basic_ostream<_E, _Tr>
{
public:
	basic_null_stream()
		: std::basic_ostream<_E, _Tr>(null_buf = new basic_null_streambuf<_E, _Tr>())
	{
	}
	
	virtual ~basic_null_stream()
	{
		delete null_buf;
	}

	basic_null_stream& put(_E)
	{
		// nothing to do
		return *this;
	}
	
	basic_null_stream& write(const _E *, std::streamsize)
	{
		// nothing to do
		return *this;
	}

protected:
	basic_null_streambuf<_E, _Tr> * null_buf;
};

typedef basic_null_stream<char> null_stream;

class line_number
{
public:
	virtual ~line_number()
	{
	}

	virtual uint get_line() const = 0;
};

template<class _E, class _Tr = std::char_traits<_E> >
class basic_filebuf_lc
	: public std::basic_streambuf<_E, _Tr>
	, public line_number
{
public:
	typedef std::basic_streambuf<_E, _Tr> _Mybase;
	typedef basic_filebuf_lc<_E, _Tr> _Myt;
	typedef _E char_type;
	typedef _Tr traits_type;
	typedef typename _Tr::int_type int_type;
	typedef typename _Tr::pos_type pos_type;
	typedef typename _Tr::off_type off_type;

	basic_filebuf_lc(FILE *_fp = 0)
		: _Mybase()
		, fp(_fp)
		, must_close(false)
		, fline(1)
	{
	}

	virtual ~basic_filebuf_lc()
	{
		sync();
		if (must_close)
		{
			close();
		}
	}

	uint get_line() const
	{
		return fline + 1;
	}

	bool is_open() const
	{
		return fp != 0;
	}

	_Myt* open(const char *filename, std::ios_base::openmode mode = std::ios_base::out)
	{
		if (fp != 0)
		{
			return 0;
		}
		const char * fmode = (mode & std::ios_base::out)
		                   ? "wt"
		                   : "rt";
		if ((fp = fopen(filename, fmode)) == 0)
		{
			return 0;
		}

		must_close = true;
		return this;
	}

	_Myt* open(FILE * _fp)
	{
		if (fp != 0)
		{
			return 0;
		}
		fp = _fp;
		must_close = false;
		return this;
	}

	_Myt* close()
	{
		sync();

		if (fp == 0 || fclose(fp) != 0)
		{
			fp = 0;
			return 0;
		}
		else
		{
			fp = 0;
			return this;
		}
	}

protected:

	virtual int_type overflow(int_type c = _Tr::eof())
	{
		if (c == '\n')
		{
			++fline;
		}
		if (_Tr::eq_int_type(_Tr::eof(), c))
		{
			return _Tr::not_eof(c);
		}
		else
		{
			buffer += _Tr::to_char_type(c);
			return c;
		}
	}

	virtual int_type pbackfail(int_type c = _Tr::eof())
	{
		assert(0);
		c = 0;
		return _Tr::eof();
	}

	virtual int_type underflow() // don't point past it
	{
		int c;

		if (buffer.length())
		{
			return buffer[0];
		}
		if (fp == 0 || ((c = fgetc(fp)) == EOF))
		{
			return _Tr::eof();
		}
		buffer += (char)c;
		return c;
	}

	virtual int_type uflow() // point past it
	{
		int c;

		if (buffer.length())
		{
			c = buffer[0];
			buffer.erase(0, 1);
			return c;
		}
		if (fp == 0 || ((c = fgetc(fp)) == EOF))
		{
			return _Tr::eof();
		}
		else if (c == '\n')
		{
			++fline;
		}
		return c;
	}

#if 0
	virtual std::streamsize xsgetn(_E* buf, std::streamsize n)
	{
		std::streamsize r = 0;
		while(n--)
		{
			int_type c = underflow();
			if (_Tr::eq_int_type(_Tr::eof(), c))
			{
				break;
			}
			buf[r++] = c;
		}
		buf[r] = '\0';
		return r;
	}
#endif

	virtual pos_type seekoff(off_type off, std::ios_base::seekdir whence,
		std::ios_base::openmode = (std::ios_base::openmode)(std::ios_base::in | std::ios_base::out))
	{
		return fseek(fp, (long)off, whence);
	}

	virtual pos_type seekpos(pos_type fpos,
		std::ios_base::openmode = (std::ios_base::openmode)(std::ios_base::in | std::ios_base::out))
	{
		return fseek(fp, (long)fpos, SEEK_SET);
	}

	virtual _Mybase * setbuf(_E *, std::streamsize)
	{
		assert(0);
		return this;
	}

	virtual int sync()
	{
		if (buffer.length() != 0) {
			fwrite(buffer.c_str(), sizeof(_E), buffer.length(), fp);
		}
		buffer.clear();
		return fp == 0
			|| _Tr::eq_int_type(_Tr::eof(), overflow())
			|| 0 <= fflush(fp) ? 0 : -1;
	}

	virtual std::streamsize xsputn(const _E *buf, std::streamsize cnt)
	{
		if (buffer.length() != 0) {
			fwrite(buffer.c_str(), sizeof(_E), buffer.length(), fp);
		}
		buffer.clear();
		/*fline += std::count(buf, buf + cnt, '\n');*/
		for (std::streamsize pos = 0; pos < cnt; ++pos) 
		{
			if (buf[pos] == '\n')
			{
				++fline;
			}
		}
		if (cnt != 0) {
			return fwrite(buf, sizeof(_E), cnt, fp);
		} else {
			return 0;
		}
	}

private:

	FILE * fp;
	bool   must_close;
	uint   fline;
	std::basic_string<_E, _Tr> buffer;
};

typedef basic_filebuf_lc<char> filebuf_lc;

template<
	class _E, 
	class _BaseStream,
	std::ios_base::openmode  _DefOpenMode,
	class _Tr = std::char_traits<_E> >
class basic_fstream_lc
	: public _BaseStream
	, public line_number
{
public:
	typedef basic_fstream_lc<_E, _BaseStream, _DefOpenMode, _Tr> _Myt;
	typedef std::basic_ios<_E, _Tr> _Myios;
	typedef _BaseStream _Mybase;
	typedef basic_filebuf_lc<_E, _Tr> _Mybuf;

	basic_fstream_lc()
		: _Mybase(mybuf = new _Mybuf())
	{
	}

	virtual ~basic_fstream_lc()
	{
		delete mybuf;
	}

	bool is_open() const
	{
		return mybuf->is_open();
	}

	_Myt& open(const char * filename, std::ios_base::openmode mode = _DefOpenMode)
	{
		if ((mode & _DefOpenMode) == 0 || mybuf->open(filename, mode) == 0)
		{
			_Myios::setstate(std::ios_base::failbit);
		}
		return *this;
	}
	
	_Myt& open(FILE *fp)
	{
		if (mybuf->open(fp) == 0)
		{
			_Myios::setstate(std::ios_base::failbit);
		}
		return *this;
	}
	
	void close()
	{
		if (mybuf->close() == 0)
		{
			_Myios::setstate(std::ios_base::failbit);
		}
	}
	
	uint get_line() const
	{
		return mybuf->get_line();
	}

protected:
	mutable _Mybuf *mybuf;
};

template<class _E, class _Tr = std::char_traits<_E> >
class basic_ofstream_lc
	: public basic_fstream_lc<_E, std::basic_ostream<_E, _Tr>, std::ios_base::out, _Tr>
{
};

typedef basic_ofstream_lc<char> ofstream_lc;

template<class _E, class _Tr = std::char_traits<_E> >
class basic_ifstream_lc
	: public basic_fstream_lc<_E, std::basic_istream<_E, _Tr>, std::ios_base::in, _Tr>
{
};

typedef basic_ifstream_lc<char> ifstream_lc;

class file_info
{
public:

	static std::string escape(const std::string& _str)
	{
		std::string str(_str);
		size_t l = str.length();
		for (size_t p = 0; p < l; ++p)
		{
			if (str[p] == '\\')
			{
				str.insert(++p, "\\");
				++l;
			}
		}
		return str;
	}

	file_info()
		: ln(NULL)
	{
	}

	file_info(const std::string& _fname, const line_number* _ln, bool _escape = true)
		: fname(_escape ? escape(_fname) : _fname)
		, ln(_ln)
	{
	}

	file_info(const file_info& oth, const line_number* _ln = NULL)
		: fname(oth.fname)
		, ln(_ln)
	{
	}

	file_info& operator = (const file_info& oth)
	{
		*(const_cast<std::string*>(&this->fname)) = oth.fname;
		ln = oth.ln;
		return *this;
	}

	const std::string  fname;
	const line_number* ln;
};

std::ostream& operator << (std::ostream& o, const file_info& li);

} // end namespace re2c

#endif /* _stream_lc_h */
