#ifndef LIBXMP_FORMAT_H
#define LIBXMP_FORMAT_H

#include "common.h"
#include "hio.h"

struct format_loader {
	const char *name;
	int (*test)(HIO_HANDLE *, char *, const int);
	int (*loader)(struct module_data *, HIO_HANDLE *, const int);
};

extern const struct format_loader *const format_loaders[];

const char *const *format_list(void);

extern const struct format_loader libxmp_loader_xm;
extern const struct format_loader libxmp_loader_mod;
extern const struct format_loader libxmp_loader_it;
extern const struct format_loader libxmp_loader_s3m;

#ifndef LIBXMP_CORE_PLAYER
extern const struct format_loader libxmp_loader_flt;
extern const struct format_loader libxmp_loader_st;
extern const struct format_loader libxmp_loader_stm;
extern const struct format_loader libxmp_loader_stx;
extern const struct format_loader libxmp_loader_mtm;
extern const struct format_loader libxmp_loader_ice;
extern const struct format_loader libxmp_loader_imf;
extern const struct format_loader libxmp_loader_ptm;
extern const struct format_loader libxmp_loader_mdl;
extern const struct format_loader libxmp_loader_ult;
extern const struct format_loader libxmp_loader_liq;
extern const struct format_loader libxmp_loader_no;
extern const struct format_loader libxmp_loader_masi;
extern const struct format_loader libxmp_loader_masi16;
extern const struct format_loader libxmp_loader_muse;
extern const struct format_loader libxmp_loader_gal5;
extern const struct format_loader libxmp_loader_gal4;
extern const struct format_loader libxmp_loader_amf;
extern const struct format_loader libxmp_loader_asylum;
extern const struct format_loader libxmp_loader_gdm;
extern const struct format_loader libxmp_loader_mmd1;
extern const struct format_loader libxmp_loader_mmd3;
extern const struct format_loader libxmp_loader_med2;
extern const struct format_loader libxmp_loader_med3;
extern const struct format_loader libxmp_loader_med4;
extern const struct format_loader libxmp_loader_rtm;
extern const struct format_loader libxmp_loader_pt3;
extern const struct format_loader libxmp_loader_dt;
extern const struct format_loader libxmp_loader_mgt;
extern const struct format_loader libxmp_loader_arch;
extern const struct format_loader libxmp_loader_sym;
extern const struct format_loader libxmp_loader_digi;
extern const struct format_loader libxmp_loader_dbm;
extern const struct format_loader libxmp_loader_emod;
extern const struct format_loader libxmp_loader_okt;
extern const struct format_loader libxmp_loader_sfx;
extern const struct format_loader libxmp_loader_far;
extern const struct format_loader libxmp_loader_umx;
extern const struct format_loader libxmp_loader_stim;
extern const struct format_loader libxmp_loader_coco;
extern const struct format_loader libxmp_loader_ims;
extern const struct format_loader libxmp_loader_669;
extern const struct format_loader libxmp_loader_fnk;
extern const struct format_loader libxmp_loader_mfp;
extern const struct format_loader libxmp_loader_pw;
extern const struct format_loader libxmp_loader_hmn;
extern const struct format_loader libxmp_loader_chip;
extern const struct format_loader libxmp_loader_abk;
extern const struct format_loader libxmp_loader_xmf;
#if 0 /* broken / unused, yet. */
extern const struct format_loader libxmp_loader_dmf;
extern const struct format_loader libxmp_loader_tcb;
extern const struct format_loader libxmp_loader_gtk;
extern const struct format_loader libxmp_loader_dtt;
extern const struct format_loader libxmp_loader_mtp;
extern const struct format_loader libxmp_loader_amd;
extern const struct format_loader libxmp_loader_rad;
extern const struct format_loader libxmp_loader_hsc;
extern const struct format_loader libxmp_loader_alm;
extern const struct format_loader libxmp_loader_polly;
extern const struct format_loader libxmp_loader_stc;
#endif
#endif /* LIBXMP_CORE_PLAYER */

#ifndef LIBXMP_CORE_PLAYER
#define NUM_FORMATS 52
#elif !defined(LIBXMP_CORE_DISABLE_IT)
#define NUM_FORMATS 4
#else
#define NUM_FORMATS 3
#endif

#ifndef LIBXMP_NO_PROWIZARD
#define NUM_PW_FORMATS 43
extern const struct pw_format *const pw_formats[];
int pw_test_format(HIO_HANDLE *, char *, const int, struct xmp_test_info *);
#else
#define NUM_PW_FORMATS 0
#endif

#endif /* LIBXMP_FORMAT_H */
