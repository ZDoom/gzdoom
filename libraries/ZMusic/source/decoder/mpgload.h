#ifndef MPGDEF_H
#define MPGDEF_H

#if defined HAVE_MPG123 && defined DYN_MPG123

#define DEFINE_ENTRY(type, name) static TReqProc<MPG123Module, type> p_##name{#name};
DEFINE_ENTRY(int (*)(mpg123_handle *mh), mpg123_close)
DEFINE_ENTRY(void (*)(mpg123_handle *mh), mpg123_delete)
DEFINE_ENTRY(int (*)(void), mpg123_init)
DEFINE_ENTRY(mpg123_handle* (*)(const char* decoder, int *error), mpg123_new)
DEFINE_ENTRY(int (*)(mpg123_handle *mh, ssize_t (*r_read) (void *, void *, size_t), off_t (*r_lseek)(void *, off_t, int), void (*cleanup)(void*)), mpg123_replace_reader_handle)
DEFINE_ENTRY(int (*)(mpg123_handle *mh, void *iohandle), mpg123_open_handle)
DEFINE_ENTRY(int (*)(mpg123_handle *mh, long *rate, int *channels, int *encoding), mpg123_getformat)
DEFINE_ENTRY(int (*)(mpg123_handle *mh), mpg123_format_none)
DEFINE_ENTRY(int (*)(mpg123_handle *mh, unsigned char *outmemory, size_t outmemsize, size_t *done), mpg123_read)
DEFINE_ENTRY(off_t (*)(mpg123_handle *mh, off_t sampleoff, int whence), mpg123_seek)
DEFINE_ENTRY(int (*)(mpg123_handle *mh, long rate, int channels, int encodings), mpg123_format)
DEFINE_ENTRY(off_t (*)(mpg123_handle *mh), mpg123_tell)
DEFINE_ENTRY(off_t (*)(mpg123_handle *mh), mpg123_length)

#undef DEFINE_ENTRY

#ifndef IN_IDE_PARSER
#define mpg123_close p_mpg123_close
#define mpg123_delete p_mpg123_delete
#define mpg123_init p_mpg123_init
#define mpg123_new p_mpg123_new
#define mpg123_replace_reader_handle p_mpg123_replace_reader_handle
#define mpg123_open_handle p_mpg123_open_handle
#define mpg123_getformat p_mpg123_getformat
#define mpg123_format_none p_mpg123_format_none
#define mpg123_read p_mpg123_read
#define mpg123_seek p_mpg123_seek
#define mpg123_tell p_mpg123_tell
#define mpg123_format p_mpg123_format
#define mpg123_length p_mpg123_length
#endif

#endif
#endif
