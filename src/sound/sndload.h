#ifndef SNDDEF_H
#define SNDDEF_H

#if defined HAVE_SNDFILE && defined DYN_SNDFILE

#define DEFINE_ENTRY(type, name) static TReqProc<SndFileModule, type> p_##name{#name};
DEFINE_ENTRY(int (*)(SNDFILE *sndfile), sf_close)
DEFINE_ENTRY(SNDFILE* (*)(SF_VIRTUAL_IO *sfvirtual, int mode, SF_INFO *sfinfo, void *user_data), sf_open_virtual)
DEFINE_ENTRY(sf_count_t (*)(SNDFILE *sndfile, float *ptr, sf_count_t frames), sf_readf_float)
DEFINE_ENTRY(sf_count_t (*)(SNDFILE *sndfile, sf_count_t frames, int whence), sf_seek)
#undef DEFINE_ENTRY

#ifndef IN_IDE_PARSER
#define sf_close p_sf_close
#define sf_open_virtual p_sf_open_virtual
#define sf_readf_float p_sf_readf_float
#define sf_seek p_sf_seek
#endif

#endif
#endif


