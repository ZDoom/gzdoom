/*
 wildmidi_lib.c

 Midi Wavetable Processing library

 Copyright (C) Chris Ison  2001-2014
 Copyright (C) Bret Curtis 2013-2014

 This file is part of WildMIDI.

 WildMIDI is free software: you can redistribute and/or modify the player
 under the terms of the GNU General Public License and you can redistribute
 and/or modify the library under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation, either version 3 of
 the licenses, or(at your option) any later version.

 WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 the GNU Lesser General Public License for more details.

 You should have received a copy of the GNU General Public License and the
 GNU Lesser General Public License along with WildMIDI.  If not,  see
 <http://www.gnu.org/licenses/>.
 */

//#include "config.h"

#define UNUSED(x) (void)(x)

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#ifndef _WIN32
#include <pwd.h>
#include <strings.h>
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common.h"
#include "wm_error.h"
#include "file_io.h"
#include "lock.h"
#include "reverb.h"
#include "gus_pat.h"
#include "wildmidi_lib.h"

#define IS_DIR_SEPARATOR(c)	((c) == '/' || (c) == '\\')
#ifdef _WIN32
#define HAS_DRIVE_SPEC(f)	((f)[0] && ((f)[1] == ':'))
#else
#define HAS_DRIVE_SPEC(f)	(0)
#endif
#define IS_ABSOLUTE_PATH(f)	(IS_DIR_SEPARATOR((f)[0]) || HAS_DRIVE_SPEC((f)))


/*
 * =========================
 * Global Data and Data Structs
 * =========================
 */

#define MEM_CHUNK 8192

static int WM_Initialized = 0;
static signed short int WM_MasterVolume = 948;
static unsigned short int WM_MixerOptions = 0;

static char WM_Version[] = "WildMidi Processing Library";

unsigned short int _WM_SampleRate;

static struct _patch *patch[128];

static float reverb_room_width = 16.875f;
static float reverb_room_length = 22.5f;

static float reverb_listen_posx = 8.4375f;
static float reverb_listen_posy = 16.875f;

static int fix_release = 0;
static int auto_amp = 0;
static int auto_amp_with_amp = 0;

static int patch_lock;

struct _channel {
	unsigned char bank;
	struct _patch *patch;
	unsigned char hold;
	unsigned char volume;
	unsigned char pressure;
	unsigned char expression;
	signed char balance;
	signed char pan;
	signed short int left_adjust;
	signed short int right_adjust;
	signed short int pitch;
	signed short int pitch_range;
	signed long int pitch_adjust;
	unsigned short reg_data;
	unsigned char reg_non;
	unsigned char isdrum;
};

#define HOLD_OFF 0x02

struct _note {
	unsigned short noteid;
	unsigned char velocity;
	struct _patch *patch;
	struct _sample *sample;
	unsigned long int sample_pos;
	unsigned long int sample_inc;
	signed long int env_inc;
	unsigned char env;
	signed long int env_level;
	unsigned char modes;
	unsigned char hold;
	unsigned char active;
	struct _note *replay;
	struct _note *next;
	unsigned long int vol_lvl;
	unsigned char is_off;
};

struct _miditrack {
	unsigned long int length;
	unsigned long int ptr;
	unsigned long int delta;
	unsigned char running_event;
	unsigned char EOT;
};

struct _mdi_patches {
	struct _patch *patch;
	struct _mdi_patch *next;
};

struct _event_data {
	unsigned char channel;
	unsigned long int data;
};

struct _mdi {
	int lock;
	unsigned long int samples_to_mix;
	struct _event *events;
	struct _event *current_event;
	unsigned long int event_count;
	unsigned long int events_size;	/* try to stay optimally ahead to prevent reallocs */

	unsigned short midi_master_vol;
	struct _WM_Info info;
	struct _WM_Info *tmp_info;
	struct _channel channel[16];
	struct _note *note;
	struct _note note_table[2][16][128];

	struct _patch **patches;
	unsigned long int patch_count;
	signed short int amp;

	signed int *mix_buffer;
	unsigned long int mix_buffer_size;

	struct _rvb *reverb;
};

struct _event {
	void (*do_event)(struct _mdi *mdi, struct _event_data *data);
	struct _event_data event_data;
	unsigned long int samples_to_next;
	unsigned long int samples_to_next_fixed;
};

#define FPBITS 10
#define FPMASK ((1L<<FPBITS)-1L)

/* Gauss Interpolation code adapted from code supplied by Eric. A. Welsh */
static double newt_coeffs[58][58];	/* for start/end of samples */
#define MAX_GAUSS_ORDER 34		/* 34 is as high as we can go before errors crop up */
static double *gauss_table = NULL;	/* *gauss_table[1<<FPBITS] */
static int gauss_n = MAX_GAUSS_ORDER;
static int gauss_lock;

static void init_gauss(void) {
	/* init gauss table */
	int n = gauss_n;
	int m, i, k, n_half = (n >> 1);
	int j;
	int sign;
	double ck;
	double x, x_inc, xz;
	double z[35];
	double *gptr, *t;

	_WM_Lock(&gauss_lock);
	if (gauss_table) {
		_WM_Unlock(&gauss_lock);
		return;
	}

	newt_coeffs[0][0] = 1;
	for (i = 0; i <= n; i++) {
		newt_coeffs[i][0] = 1;
		newt_coeffs[i][i] = 1;

		if (i > 1) {
			newt_coeffs[i][0] = newt_coeffs[i - 1][0] / i;
			newt_coeffs[i][i] = newt_coeffs[i - 1][0] / i;
		}

		for (j = 1; j < i; j++) {
			newt_coeffs[i][j] = newt_coeffs[i - 1][j - 1]
					+ newt_coeffs[i - 1][j];
			if (i > 1)
				newt_coeffs[i][j] /= i;
		}
		z[i] = i / (4 * M_PI);
	}

	for (i = 0; i <= n; i++)
		for (j = 0, sign = (int)pow(-1., i); j <= i; j++, sign *= -1)
			newt_coeffs[i][j] *= sign;

	t = (double*)malloc((1<<FPBITS) * (n + 1) * sizeof(double));
	x_inc = 1.0 / (1<<FPBITS);
	for (m = 0, x = 0.0; m < (1<<FPBITS); m++, x += x_inc) {
		xz = (x + n_half) / (4 * M_PI);
		gptr = &t[m * (n + 1)];

		for (k = 0; k <= n; k++) {
			ck = 1.0;

			for (i = 0; i <= n; i++) {
				if (i == k)
					continue;

				ck *= (sin(xz - z[i])) / (sin(z[k] - z[i]));
			}
			*gptr++ = ck;
		}
	}

	gauss_table = t;
	_WM_Unlock(&gauss_lock);
}

static void free_gauss(void) {
	_WM_Lock(&gauss_lock);
	free(gauss_table);
	gauss_table = NULL;
	_WM_Unlock(&gauss_lock);
}

struct _hndl {
	void * handle;
	struct _hndl *next;
	struct _hndl *prev;
};

static struct _hndl *first_handle = NULL;

/* f: ( VOLUME / 127.0 ) * 1024.0 */
static signed short int lin_volume[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72,
		80, 88, 96, 104, 112, 120, 129, 137, 145, 153, 161, 169, 177, 185, 193,
		201, 209, 217, 225, 233, 241, 249, 258, 266, 274, 282, 290, 298, 306,
		314, 322, 330, 338, 346, 354, 362, 370, 378, 387, 395, 403, 411, 419,
		427, 435, 443, 451, 459, 467, 475, 483, 491, 499, 507, 516, 524, 532,
		540, 548, 556, 564, 572, 580, 588, 596, 604, 612, 620, 628, 636, 645,
		653, 661, 669, 677, 685, 693, 701, 709, 717, 725, 733, 741, 749, 757,
		765, 774, 782, 790, 798, 806, 814, 822, 830, 838, 846, 854, 862, 870,
		878, 886, 894, 903, 911, 919, 927, 935, 943, 951, 959, 967, 975, 983,
		991, 999, 1007, 1015, 1024 };

/* f: pow(( VOLUME / 127.0 ), 2.0 ) * 1024.0 */
static signed short int sqr_volume[] = { 0, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 7, 9,
		10, 12, 14, 16, 18, 20, 22, 25, 27, 30, 33, 36, 39, 42, 46, 49, 53, 57,
		61, 65, 69, 73, 77, 82, 86, 91, 96, 101, 106, 111, 117, 122, 128, 134,
		140, 146, 152, 158, 165, 171, 178, 185, 192, 199, 206, 213, 221, 228,
		236, 244, 251, 260, 268, 276, 284, 293, 302, 311, 320, 329, 338, 347,
		357, 366, 376, 386, 396, 406, 416, 426, 437, 447, 458, 469, 480, 491,
		502, 514, 525, 537, 549, 560, 572, 585, 597, 609, 622, 634, 647, 660,
		673, 686, 699, 713, 726, 740, 754, 768, 782, 796, 810, 825, 839, 854,
		869, 884, 899, 914, 929, 944, 960, 976, 992, 1007, 1024 };

/* f: pow(( VOLUME / 127.0 ), 0.5 ) * 1024.0 */
static signed short int pan_volume[] = { 0, 90, 128, 157, 181, 203, 222, 240,
		257, 272, 287, 301, 314, 327, 339, 351, 363, 374, 385, 396, 406, 416,
		426, 435, 445, 454, 463, 472, 480, 489, 497, 505, 514, 521, 529, 537,
		545, 552, 560, 567, 574, 581, 588, 595, 602, 609, 616, 622, 629, 636,
		642, 648, 655, 661, 667, 673, 679, 686, 692, 697, 703, 709, 715, 721,
		726, 732, 738, 743, 749, 754, 760, 765, 771, 776, 781, 786, 792, 797,
		802, 807, 812, 817, 822, 827, 832, 837, 842, 847, 852, 857, 862, 866,
		871, 876, 880, 885, 890, 894, 899, 904, 908, 913, 917, 922, 926, 931,
		935, 939, 944, 948, 953, 957, 961, 965, 970, 974, 978, 982, 987, 991,
		995, 999, 1003, 1007, 1011, 1015, 1019, 1024 };

static unsigned long int freq_table[] = { 837201792, 837685632, 838169728,
		838653568, 839138240, 839623232, 840108480, 840593984, 841079680,
		841565184, 842051648, 842538240, 843025152, 843512320, 843999232,
		844486976, 844975040, 845463360, 845951936, 846440320, 846929536,
		847418944, 847908608, 848398656, 848888960, 849378944, 849869824,
		850361024, 850852416, 851344192, 851835584, 852327872, 852820480,
		853313280, 853806464, 854299328, 854793024, 855287040, 855781312,
		856275904, 856770752, 857265344, 857760704, 858256448, 858752448,
		859248704, 859744768, 860241600, 860738752, 861236160, 861733888,
		862231360, 862729600, 863228160, 863727104, 864226176, 864725696,
		865224896, 865724864, 866225152, 866725760, 867226688, 867727296,
		868228736, 868730496, 869232576, 869734912, 870236928, 870739904,
		871243072, 871746560, 872250368, 872754496, 873258240, 873762880,
		874267840, 874773184, 875278720, 875783936, 876290112, 876796480,
		877303232, 877810176, 878317504, 878824512, 879332416, 879840576,
		880349056, 880857792, 881366272, 881875712, 882385280, 882895296,
		883405440, 883915456, 884426304, 884937408, 885448832, 885960512,
		886472512, 886984192, 887496768, 888009728, 888522944, 889036352,
		889549632, 890063680, 890578048, 891092736, 891607680, 892122368,
		892637952, 893153792, 893670016, 894186496, 894703232, 895219648,
		895737024, 896254720, 896772672, 897290880, 897808896, 898327744,
		898846912, 899366336, 899886144, 900405568, 900925952, 901446592,
		901967552, 902488768, 903010368, 903531584, 904053760, 904576256,
		905099008, 905622016, 906144896, 906668480, 907192512, 907716800,
		908241408, 908765632, 909290816, 909816256, 910342144, 910868160,
		911394624, 911920768, 912447680, 912975104, 913502720, 914030592,
		914558208, 915086784, 915615552, 916144768, 916674176, 917203968,
		917733440, 918263744, 918794496, 919325440, 919856704, 920387712,
		920919616, 921451840, 921984320, 922517184, 923049728, 923583168,
		924116928, 924651008, 925185344, 925720000, 926254336, 926789696,
		927325312, 927861120, 928397440, 928933376, 929470208, 930007296,
		930544768, 931082560, 931619968, 932158464, 932697152, 933236160,
		933775488, 934315072, 934854464, 935394688, 935935296, 936476224,
		937017344, 937558208, 938100160, 938642304, 939184640, 939727488,
		940269888, 940813312, 941357056, 941900992, 942445440, 942990016,
		943534400, 944079680, 944625280, 945171200, 945717440, 946263360,
		946810176, 947357376, 947904832, 948452672, 949000192, 949548608,
		950097280, 950646400, 951195776, 951745472, 952294912, 952845184,
		953395904, 953946880, 954498176, 955049216, 955601088, 956153408,
		956705920, 957258816, 957812032, 958364928, 958918848, 959472960,
		960027456, 960582272, 961136768, 961692224, 962248000, 962804032,
		963360448, 963916608, 964473600, 965031040, 965588736, 966146816,
		966705152, 967263168, 967822144, 968381440, 968941120, 969501056,
		970060736, 970621376, 971182272, 971743488, 972305088, 972866368,
		973428608, 973991104, 974554048, 975117312, 975680768, 976243968,
		976808192, 977372736, 977937536, 978502656, 979067584, 979633344,
		980199488, 980765888, 981332736, 981899200, 982466688, 983034432,
		983602624, 984171008, 984739776, 985308160, 985877632, 986447360,
		987017472, 987587904, 988157952, 988729088, 989300416, 989872192,
		990444224, 991016000, 991588672, 992161728, 992735168, 993308864,
		993882880, 994456576, 995031296, 995606336, 996181696, 996757440,
		997332800, 997909184, 998485888, 999062912, 999640256, 1000217984,
		1000795392, 1001373696, 1001952448, 1002531520, 1003110848, 1003689920,
		1004270016, 1004850304, 1005431040, 1006012160, 1006592832, 1007174592,
		1007756608, 1008339008, 1008921792, 1009504768, 1010087552, 1010671296,
		1011255360, 1011839808, 1012424576, 1013009024, 1013594368, 1014180160,
		1014766272, 1015352768, 1015938880, 1016526016, 1017113472, 1017701248,
		1018289408, 1018877824, 1019465984, 1020055104, 1020644672, 1021234496,
		1021824768, 1022414528, 1023005440, 1023596608, 1024188160, 1024780096,
		1025371584, 1025964160, 1026557120, 1027150336, 1027744000, 1028337920,
		1028931520, 1029526144, 1030121152, 1030716480, 1031312128, 1031907456,
		1032503808, 1033100480, 1033697536, 1034294912, 1034892032, 1035490048,
		1036088512, 1036687232, 1037286336, 1037885824, 1038484928, 1039085056,
		1039685632, 1040286464, 1040887680, 1041488448, 1042090368, 1042692608,
		1043295168, 1043898176, 1044501440, 1045104384, 1045708288, 1046312640,
		1046917376, 1047522368, 1048127040, 1048732800, 1049338816, 1049945280,
		1050552128, 1051158528, 1051765952, 1052373824, 1052982016, 1053590592,
		1054199424, 1054807936, 1055417600, 1056027456, 1056637760, 1057248448,
		1057858752, 1058470016, 1059081728, 1059693824, 1060306304, 1060918336,
		1061531392, 1062144896, 1062758656, 1063372928, 1063987392, 1064601664,
		1065216896, 1065832448, 1066448448, 1067064704, 1067680704, 1068297728,
		1068915136, 1069532864, 1070150976, 1070768640, 1071387520, 1072006720,
		1072626240, 1073246080, 1073866368, 1074486272, 1075107200, 1075728512,
		1076350208, 1076972160, 1077593856, 1078216704, 1078839680, 1079463296,
		1080087040, 1080710528, 1081335168, 1081960064, 1082585344, 1083211008,
		1083836928, 1084462592, 1085089280, 1085716352, 1086343936, 1086971648,
		1087599104, 1088227712, 1088856576, 1089485824, 1090115456, 1090745472,
		1091375104, 1092005760, 1092636928, 1093268352, 1093900160, 1094531584,
		1095164160, 1095796992, 1096430336, 1097064064, 1097697280, 1098331648,
		1098966400, 1099601536, 1100237056, 1100872832, 1101508224, 1102144768,
		1102781824, 1103419136, 1104056832, 1104694144, 1105332608, 1105971328,
		1106610432, 1107249920, 1107889152, 1108529408, 1109170048, 1109811072,
		1110452352, 1111094144, 1111735552, 1112377984, 1113020928, 1113664128,
		1114307712, 1114950912, 1115595264, 1116240000, 1116885120, 1117530624,
		1118175744, 1118821888, 1119468416, 1120115456, 1120762752, 1121410432,
		1122057856, 1122706176, 1123355136, 1124004224, 1124653824, 1125303040,
		1125953408, 1126604160, 1127255168, 1127906560, 1128557696, 1129209984,
		1129862528, 1130515456, 1131168768, 1131822592, 1132475904, 1133130368,
		1133785216, 1134440448, 1135096064, 1135751296, 1136407680, 1137064448,
		1137721472, 1138379008, 1139036800, 1139694336, 1140353024, 1141012096,
		1141671424, 1142331264, 1142990592, 1143651200, 1144312192, 1144973440,
		1145635200, 1146296448, 1146958976, 1147621760, 1148285056, 1148948608,
		1149612672, 1150276224, 1150940928, 1151606144, 1152271616, 1152937600,
		1153603072, 1154269824, 1154936832, 1155604352, 1156272128, 1156939648,
		1157608192, 1158277248, 1158946560, 1159616384, 1160286464, 1160956288,
		1161627264, 1162298624, 1162970240, 1163642368, 1164314112, 1164987008,
		1165660160, 1166333824, 1167007872, 1167681536, 1168356352, 1169031552,
		1169707136, 1170383104, 1171059584, 1171735552, 1172412672, 1173090304,
		1173768192, 1174446592, 1175124480, 1175803648, 1176483072, 1177163008,
		1177843328, 1178523264, 1179204352, 1179885824, 1180567680, 1181249920,
		1181932544, 1182614912, 1183298304, 1183982208, 1184666368, 1185351040,
		1186035328, 1186720640, 1187406464, 1188092672, 1188779264, 1189466368,
		1190152960, 1190840832, 1191528960, 1192217600, 1192906624, 1193595136,
		1194285056, 1194975232, 1195665792, 1196356736, 1197047296, 1197739136,
		1198431360, 1199123968, 1199816960, 1200510336, 1201203328, 1201897600,
		1202592128, 1203287040, 1203982464, 1204677504, 1205373696, 1206070272,
		1206767232, 1207464704, 1208161664, 1208859904, 1209558528, 1210257536,
		1210956928, 1211656832, 1212356224, 1213056768, 1213757952, 1214459392,
		1215161216, 1215862656, 1216565376, 1217268352, 1217971840, 1218675712,
		1219379200, 1220083840, 1220788992, 1221494528, 1222200448, 1222906752,
		1223612672, 1224319872, 1225027456, 1225735424, 1226443648, 1227151616,
		1227860864, 1228570496, 1229280512, 1229990912, 1230700928, 1231412096,
		1232123776, 1232835840, 1233548288, 1234261248, 1234973696, 1235687424,
		1236401536, 1237116032, 1237831040, 1238545536, 1239261312, 1239977472,
		1240694144, 1241411072, 1242128512, 1242845568, 1243563776, 1244282496,
		1245001600, 1245721088, 1246440192, 1247160448, 1247881216, 1248602368,
		1249324032, 1250045184, 1250767616, 1251490432, 1252213632, 1252937344,
		1253661440, 1254385152, 1255110016, 1255835392, 1256561152, 1257287424,
		1258013184, 1258740096, 1259467648, 1260195456, 1260923648, 1261651584,
		1262380800, 1263110272, 1263840256, 1264570624, 1265301504, 1266031872,
		1266763520, 1267495552, 1268227968, 1268961024, 1269693440, 1270427264,
		1271161472, 1271896064, 1272631168, 1273365760, 1274101632, 1274838016,
		1275574784, 1276311808, 1277049472, 1277786624, 1278525056, 1279264000,
		1280003328, 1280743040, 1281482368, 1282222976, 1282963968, 1283705344,
		1284447232, 1285188736, 1285931392, 1286674560, 1287418240, 1288162176,
		1288906624, 1289650688, 1290395904, 1291141760, 1291887872, 1292634496,
		1293380608, 1294128128, 1294875904, 1295624320, 1296373120, 1297122304,
		1297870976, 1298621056, 1299371520, 1300122496, 1300873856, 1301624832,
		1302376960, 1303129600, 1303882752, 1304636288, 1305389312, 1306143872,
		1306898688, 1307654016, 1308409600, 1309165696, 1309921536, 1310678528,
		1311435904, 1312193920, 1312952192, 1313710080, 1314469248, 1315228928,
		1315988992, 1316749568, 1317509632, 1318271104, 1319032960, 1319795200,
		1320557952, 1321321088, 1322083840, 1322847872, 1323612416, 1324377216,
		1325142656, 1325907584, 1326673920, 1327440512, 1328207744, 1328975360,
		1329742464, 1330510976, 1331279872, 1332049152, 1332819072, 1333589248,
		1334359168, 1335130240, 1335901824, 1336673920, 1337446400, 1338218368,
		1338991744, 1339765632, 1340539904, 1341314560, 1342088832, 1342864512,
		1343640576, 1344417024, 1345193984, 1345971456, 1346748416, 1347526656,
		1348305408, 1349084672, 1349864320, 1350643456, 1351424000, 1352205056,
		1352986496, 1353768448, 1354550784, 1355332608, 1356115968, 1356899712,
		1357683840, 1358468480, 1359252608, 1360038144, 1360824192, 1361610624,
		1362397440, 1363183872, 1363971712, 1364760064, 1365548672, 1366337792,
		1367127424, 1367916672, 1368707200, 1369498240, 1370289664, 1371081472,
		1371873024, 1372665856, 1373459072, 1374252800, 1375047040, 1375840768,
		1376635904, 1377431552, 1378227584, 1379024000, 1379820928, 1380617472,
		1381415296, 1382213760, 1383012480, 1383811840, 1384610560, 1385410816,
		1386211456, 1387012480, 1387814144, 1388615168, 1389417728, 1390220672,
		1391024128, 1391827968, 1392632320, 1393436288, 1394241536, 1395047296,
		1395853568, 1396660224, 1397466368, 1398274048, 1399082112, 1399890688,
		1400699648, 1401508224, 1402318080, 1403128576, 1403939456, 1404750848,
		1405562624, 1406374016, 1407186816, 1408000000, 1408813696, 1409627904,
		1410441728, 1411256704, 1412072320, 1412888320, 1413704960, 1414521856,
		1415338368, 1416156288, 1416974720, 1417793664, 1418612992, 1419431808,
		1420252160, 1421072896, 1421894144, 1422715904, 1423537280, 1424359808,
		1425183104, 1426006784, 1426830848, 1427655296, 1428479488, 1429305088,
		1430131072, 1430957568, 1431784576, 1432611072, 1433438976, 1434267392,
		1435096192, 1435925632, 1436754432, 1437584768, 1438415616, 1439246848,
		1440078720, 1440910848, 1441742720, 1442575872, 1443409664, 1444243584,
		1445078400, 1445912576, 1446748032, 1447584256, 1448420864, 1449257856,
		1450094464, 1450932480, 1451771008, 1452609920, 1453449472, 1454289408,
		1455128960, 1455969920, 1456811264, 1457653248, 1458495616, 1459337600,
		1460180864, 1461024768, 1461869056, 1462713984, 1463558272, 1464404096,
		1465250304, 1466097152, 1466944384, 1467792128, 1468639488, 1469488256,
		1470337408, 1471187200, 1472037376, 1472887168, 1473738368, 1474589952,
		1475442304, 1476294912, 1477148160, 1478000768, 1478854912, 1479709696,
		1480564608, 1481420288, 1482275456, 1483132160, 1483989248, 1484846976,
		1485704960, 1486562688, 1487421696, 1488281344, 1489141504, 1490002048,
		1490863104, 1491723776, 1492585856, 1493448448, 1494311424, 1495175040,
		1496038144, 1496902656, 1497767808, 1498633344, 1499499392, 1500365056,
		1501232128, 1502099712, 1502967808, 1503836416, 1504705536, 1505574016,
		1506444032, 1507314688, 1508185856, 1509057408, 1509928576, 1510801280,
		1511674240, 1512547840, 1513421952, 1514295680, 1515170816, 1516046464,
		1516922624, 1517799296, 1518676224, 1519552896, 1520431104, 1521309824,
		1522188928, 1523068800, 1523948032, 1524828672, 1525709824, 1526591616,
		1527473792, 1528355456, 1529238784, 1530122496, 1531006720, 1531891712,
		1532776832, 1533661824, 1534547968, 1535434880, 1536322304, 1537210112,
		1538097408, 1538986368, 1539875840, 1540765696, 1541656192, 1542547072,
		1543437440, 1544329472, 1545221888, 1546114944, 1547008384, 1547901440,
		1548796032, 1549691136, 1550586624, 1551482752, 1552378368, 1553275520,
		1554173184, 1555071232, 1555970048, 1556869248, 1557767936, 1558668288,
		1559568896, 1560470272, 1561372032, 1562273408, 1563176320, 1564079616,
		1564983424, 1565888000, 1566791808, 1567697408, 1568603392, 1569509760,
		1570416896, 1571324416, 1572231424, 1573140096, 1574049152, 1574958976,
		1575869184, 1576778752, 1577689984, 1578601728, 1579514112, 1580426880,
		1581339264, 1582253056, 1583167488, 1584082432, 1584997888, 1585913984,
		1586829440, 1587746304, 1588663936, 1589582080, 1590500736, 1591418880,
		1592338560, 1593258752, 1594179584, 1595100928, 1596021632, 1596944000,
		1597866880, 1598790272, 1599714304, 1600638848, 1601562752, 1602488320,
		1603414272, 1604340992, 1605268224, 1606194816, 1607123072, 1608051968,
		1608981120, 1609911040, 1610841344, 1611771264, 1612702848, 1613634688,
		1614567168, 1615500288, 1616432896, 1617367040, 1618301824, 1619237120,
		1620172800, 1621108096, 1622044928, 1622982272, 1623920128, 1624858752,
		1625797632, 1626736256, 1627676416, 1628616960, 1629558272, 1630499968,
		1631441152, 1632384000, 1633327232, 1634271232, 1635215744, 1636159744,
		1637105152, 1638051328, 1638998016, 1639945088, 1640892928, 1641840128,
		1642788992, 1643738368, 1644688384, 1645638784, 1646588672, 1647540352,
		1648492416, 1649445120, 1650398464, 1651351168, 1652305408, 1653260288,
		1654215808, 1655171712, 1656128256, 1657084288, 1658041856, 1659000064,
		1659958784, 1660918272, 1661876992, 1662837376, 1663798400, 1664759936,
		1665721984, 1666683520, 1667646720, 1668610560, 1669574784, 1670539776,
		1671505024, 1672470016, 1673436544, };

#ifdef DEBUG_MIDI
#define MIDI_EVENT_DEBUG(dx,dy) printf("\r%s, %x\n",dx,dy)
#else
#define MIDI_EVENT_DEBUG(dx,dy)
#endif

#define MAX_AUTO_AMP 2.0

/*
 * =========================
 * Internal Functions
 * =========================
 */

static void WM_CheckEventMemoryPool(struct _mdi *mdi) {
	if (mdi->event_count >= mdi->events_size) {
		mdi->events_size += MEM_CHUNK;
		mdi->events = (struct _event*)realloc(mdi->events,
			(mdi->events_size * sizeof(struct _event)));
	}
}

static void WM_InitPatches(void) {
	int i;
	for (i = 0; i < 128; i++) {
		patch[i] = NULL;
	}
}

static void WM_FreePatches(void) {
	int i;
	struct _patch * tmp_patch;
	struct _sample * tmp_sample;

	_WM_Lock(&patch_lock);
	for (i = 0; i < 128; i++) {
		while (patch[i]) {
			while (patch[i]->first_sample) {
				tmp_sample = patch[i]->first_sample->next;
				free(patch[i]->first_sample->data);
				free(patch[i]->first_sample);
				patch[i]->first_sample = tmp_sample;
			}
			free(patch[i]->filename);
			tmp_patch = patch[i]->next;
			free(patch[i]);
			patch[i] = tmp_patch;
		}
	}
	_WM_Unlock(&patch_lock);
}

/* wm_strdup -- adds extra space for appending up to 4 chars */
static char *wm_strdup (const char *str) {
	size_t l = strlen(str) + 5;
	char *d = (char *) malloc(l * sizeof(char));
	if (d) {
		strcpy(d, str);
		return d;
	}
	return NULL;
}

static inline int wm_isdigit(int c) {
	return (c >= '0' && c <= '9');
}

#define TOKEN_CNT_INC 8
static char** WM_LC_Tokenize_Line(char *line_data) {
	int line_length = strlen(line_data);
	int token_data_length = 0;
	int line_ofs = 0;
	int token_start = 0;
	char **token_data = NULL;
	int token_count = 0;

	if (line_length == 0)
		return NULL;

	do {
		/* ignore everything after #  */
		if (line_data[line_ofs] == '#') {
			break;
		}

		if ((line_data[line_ofs] == ' ') || (line_data[line_ofs] == '\t')) {
			/* whitespace means we aren't in a token */
			if (token_start) {
				token_start = 0;
				line_data[line_ofs] = '\0';
			}
		} else {
			if (!token_start) {
				/* the start of a token in the line */
				token_start = 1;
				if (token_count >= token_data_length) {
					token_data_length += TOKEN_CNT_INC;
					token_data = (char**)realloc(token_data, token_data_length * sizeof(char *));
					if (token_data == NULL) {
						_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,"to parse config", errno);
						return NULL;
					}
				}

				token_data[token_count] = &line_data[line_ofs];
				token_count++;
			}
		}
		line_ofs++;
	} while (line_ofs != line_length);

	/* if we have found some tokens then add a null token to the end */
	if (token_count) {
		if (token_count >= token_data_length) {
			token_data = (char**)realloc(token_data,
				((token_count + 1) * sizeof(char *)));
		}
		token_data[token_count] = NULL;
	}

	return token_data;
}

static int WM_LoadConfig(const char *config_file) {
	unsigned long int config_size = 0;
	char *config_buffer = NULL;
	const char *dir_end = NULL;
	char *config_dir = NULL;
	unsigned long int config_ptr = 0;
	unsigned long int line_start_ptr = 0;
	unsigned short int patchid = 0;
	struct _patch * tmp_patch;
	char **line_tokens = NULL;
	int token_count = 0;

	config_buffer = (char *) _WM_BufferFile(config_file, &config_size);
	if (!config_buffer) {
		WM_FreePatches();
		return -1;
	}


	// This part was rewritten because the original depended on a header that was GPL'd.
	dir_end = strrchr(config_file, '/');
#ifdef _WIN32
	const char *dir_end2 = strrchr(config_file, '\\');
	if (dir_end2 > dir_end) dir_end = dir_end2;
#endif

	if (dir_end) {
		config_dir = (char*)malloc((dir_end - config_file + 2));
		if (config_dir == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse config",
					errno);
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, config_file, 0);
			WM_FreePatches();
			free(config_buffer);
			return -1;
		}
		strncpy(config_dir, config_file, (dir_end - config_file + 1));
		config_dir[dir_end - config_file + 1] = '\0';
	}

	config_ptr = 0;
	line_start_ptr = 0;

	/* handle files without a newline at the end: this relies on
	 * _WM_BufferFile() allocating the buffer with one extra byte */
	config_buffer[config_size] = '\n';

	while (config_ptr <= config_size) {
		if (config_buffer[config_ptr] == '\r' ||
		    config_buffer[config_ptr] == '\n')
		{
			config_buffer[config_ptr] = '\0';

			if (config_ptr != line_start_ptr) {
				line_tokens = WM_LC_Tokenize_Line(&config_buffer[line_start_ptr]);
				if (line_tokens) {
					if (stricmp(line_tokens[0], "dir") == 0) {
						free(config_dir);
						if (!line_tokens[1]) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(missing name in dir line)", 0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(line_tokens);
							free(config_buffer);
							return -1;
						} else if (!(config_dir = wm_strdup(line_tokens[1]))) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,
									"to parse config", errno);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						if (!IS_DIR_SEPARATOR(config_dir[strlen(config_dir) - 1])) {
							config_dir[strlen(config_dir) + 1] = '\0';
							config_dir[strlen(config_dir)] = '/';
						}
					} else if (stricmp(line_tokens[0], "source") == 0) {
						char *new_config = NULL;
						if (!line_tokens[1]) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(missing name in source line)", 0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(line_tokens);
							free(config_buffer);
							return -1;
						} else if (!IS_ABSOLUTE_PATH(line_tokens[1]) && config_dir) {
							new_config = (char*)malloc(
									strlen(config_dir) + strlen(line_tokens[1])
											+ 1);
							if (new_config == NULL) {
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,
										"to parse config", errno);
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
										config_file, 0);
								WM_FreePatches();
								free(config_dir);
								free(line_tokens);
								free(config_buffer);
								return -1;
							}
							strcpy(new_config, config_dir);
							strcpy(&new_config[strlen(config_dir)], line_tokens[1]);
						} else {
							if (!(new_config = wm_strdup(line_tokens[1]))) {
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,
										"to parse config", errno);
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
										config_file, 0);
								WM_FreePatches();
								free(line_tokens);
								free(config_buffer);
								return -1;
							}
						}
						if (WM_LoadConfig(new_config) == -1) {
							free(new_config);
							free(line_tokens);
							free(config_buffer);
							free(config_dir);
							return -1;
						}
						free(new_config);
					} else if (stricmp(line_tokens[0], "bank") == 0) {
						if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(syntax error in bank line)", 0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						patchid = (atoi(line_tokens[1]) & 0xFF) << 8;
					} else if (stricmp(line_tokens[0], "drumset") == 0) {
						if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(syntax error in drumset line)", 0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						patchid = ((atoi(line_tokens[1]) & 0xFF) << 8) | 0x80;
					} else if (stricmp(line_tokens[0], "reverb_room_width") == 0) {
						if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(syntax error in reverb_room_width line)",
									0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						reverb_room_width = (float) atof(line_tokens[1]);
						if (reverb_room_width < 1.0f) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(reverb_room_width < 1 meter, setting to minimum of 1 meter)",
									0);
							reverb_room_width = 1.0f;
						} else if (reverb_room_width > 100.0f) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(reverb_room_width > 100 meters, setting to maximum of 100 meters)",
									0);
							reverb_room_width = 100.0f;
						}
					} else if (stricmp(line_tokens[0], "reverb_room_length") == 0) {
						if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(syntax error in reverb_room_length line)",
									0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						reverb_room_length = (float) atof(line_tokens[1]);
						if (reverb_room_length < 1.0f) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(reverb_room_length < 1 meter, setting to minimum of 1 meter)",
									0);
							reverb_room_length = 1.0f;
						} else if (reverb_room_length > 100.0f) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(reverb_room_length > 100 meters, setting to maximum of 100 meters)",
									0);
							reverb_room_length = 100.0f;
						}
					} else if (stricmp(line_tokens[0], "reverb_listener_posx") == 0) {
						if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(syntax error in reverb_listen_posx line)",
									0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						reverb_listen_posx = (float) atof(line_tokens[1]);
						if ((reverb_listen_posx > reverb_room_width)
								|| (reverb_listen_posx < 0.0f)) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(reverb_listen_posx set outside of room)",
									0);
							reverb_listen_posx = reverb_room_width / 2.0f;
						}
					} else if (stricmp(line_tokens[0],
							"reverb_listener_posy") == 0) {
						if (!line_tokens[1] || !wm_isdigit(line_tokens[1][0])) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(syntax error in reverb_listen_posy line)",
									0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						}
						reverb_listen_posy = (float) atof(line_tokens[1]);
						if ((reverb_listen_posy > reverb_room_width)
								|| (reverb_listen_posy < 0.0f)) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(reverb_listen_posy set outside of room)",
									0);
							reverb_listen_posy = reverb_room_length * 0.75f;
						}
					} else if (stricmp(line_tokens[0],
							"guspat_editor_author_cant_read_so_fix_release_time_for_me")
							== 0) {
						fix_release = 1;
					} else if (stricmp(line_tokens[0], "auto_amp") == 0) {
						auto_amp = 1;
					} else if (stricmp(line_tokens[0], "auto_amp_with_amp")
							== 0) {
						auto_amp = 1;
						auto_amp_with_amp = 1;
					} else if (wm_isdigit(line_tokens[0][0])) {
						patchid = (patchid & 0xFF80)
								| (atoi(line_tokens[0]) & 0x7F);
						if (patch[(patchid & 0x7F)] == NULL) {
							patch[(patchid & 0x7F)] = (struct _patch*)malloc(
									sizeof(struct _patch));
							if (patch[(patchid & 0x7F)] == NULL) {
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,
										NULL, errno);
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
										config_file, 0);
								WM_FreePatches();
								free(config_dir);
								free(line_tokens);
								free(config_buffer);
								return -1;
							}
							tmp_patch = patch[(patchid & 0x7F)];
							tmp_patch->patchid = patchid;
							tmp_patch->filename = NULL;
							tmp_patch->amp = 1024;
							tmp_patch->note = 0;
							tmp_patch->next = NULL;
							tmp_patch->first_sample = NULL;
							tmp_patch->loaded = 0;
							tmp_patch->inuse_count = 0;
						} else {
							tmp_patch = patch[(patchid & 0x7F)];
							if (tmp_patch->patchid == patchid) {
								free(tmp_patch->filename);
								tmp_patch->filename = NULL;
								tmp_patch->amp = 1024;
								tmp_patch->note = 0;
							} else {
								if (tmp_patch->next) {
									while (tmp_patch->next) {
										if (tmp_patch->next->patchid == patchid)
											break;
										tmp_patch = tmp_patch->next;
									}
									if (tmp_patch->next == NULL) {
										if ((tmp_patch->next = (struct _patch*)malloc(
												sizeof(struct _patch)))
												== NULL) {
											_WM_ERROR(__FUNCTION__, __LINE__,
													WM_ERR_MEM, NULL, 0);
											_WM_ERROR(__FUNCTION__, __LINE__,
													WM_ERR_LOAD, config_file,
													0);
											WM_FreePatches();
											free(config_dir);
											free(line_tokens);
											free(config_buffer);
											return -1;
										}
										tmp_patch = tmp_patch->next;
										tmp_patch->patchid = patchid;
										tmp_patch->filename = NULL;
										tmp_patch->amp = 1024;
										tmp_patch->note = 0;
										tmp_patch->next = NULL;
										tmp_patch->first_sample = NULL;
										tmp_patch->loaded = 0;
										tmp_patch->inuse_count = 0;
									} else {
										tmp_patch = tmp_patch->next;
										free(tmp_patch->filename);
										tmp_patch->filename = NULL;
										tmp_patch->amp = 1024;
										tmp_patch->note = 0;
									}
								} else {
									tmp_patch->next = (struct _patch*)malloc(
											sizeof(struct _patch));
									if (tmp_patch->next == NULL) {
										_WM_ERROR(__FUNCTION__, __LINE__,
												WM_ERR_MEM, NULL, errno);
										_WM_ERROR(__FUNCTION__, __LINE__,
												WM_ERR_LOAD, config_file, 0);
										WM_FreePatches();
										free(config_dir);
										free(line_tokens);
										free(config_buffer);
										return -1;
									}
									tmp_patch = tmp_patch->next;
									tmp_patch->patchid = patchid;
									tmp_patch->filename = NULL;
									tmp_patch->amp = 1024;
									tmp_patch->note = 0;
									tmp_patch->next = NULL;
									tmp_patch->first_sample = NULL;
									tmp_patch->loaded = 0;
									tmp_patch->inuse_count = 0;
								}
							}
						}
						if (!line_tokens[1]) {
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
									"(missing name in patch line)", 0);
							_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
									config_file, 0);
							WM_FreePatches();
							free(config_dir);
							free(line_tokens);
							free(config_buffer);
							return -1;
						} else if (!IS_ABSOLUTE_PATH(line_tokens[1]) && config_dir) {
							tmp_patch->filename = (char*)malloc(
									strlen(config_dir) + strlen(line_tokens[1])
											+ 5);
							if (tmp_patch->filename == NULL) {
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,
										NULL, 0);
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
										config_file, 0);
								WM_FreePatches();
								free(config_dir);
								free(line_tokens);
								free(config_buffer);
								return -1;
							}
							strcpy(tmp_patch->filename, config_dir);
							strcat(tmp_patch->filename, line_tokens[1]);
						} else {
							if (!(tmp_patch->filename = wm_strdup(line_tokens[1]))) {
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM,
										NULL, 0);
								_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD,
										config_file, 0);
								WM_FreePatches();
								free(config_dir);
								free(line_tokens);
								free(config_buffer);
								return -1;
							}
						}
						if (strnicmp(
								&tmp_patch->filename[strlen(tmp_patch->filename)
										- 4], ".pat", 4) != 0) {
							strcat(tmp_patch->filename, ".pat");
						}
						tmp_patch->env[0].set = 0x00;
						tmp_patch->env[1].set = 0x00;
						tmp_patch->env[2].set = 0x00;
						tmp_patch->env[3].set = 0x00;
						tmp_patch->env[4].set = 0x00;
						tmp_patch->env[5].set = 0x00;
						tmp_patch->keep = 0;
						tmp_patch->remove = 0;

						token_count = 0;
						while (line_tokens[token_count]) {
							if (strnicmp(line_tokens[token_count], "amp=", 4)
									== 0) {
								if (!wm_isdigit(line_tokens[token_count][4])) {
									_WM_ERROR(__FUNCTION__, __LINE__,
											WM_ERR_INVALID_ARG,
											"(syntax error in patch line)", 0);
								} else {
									tmp_patch->amp = (atoi(
											&line_tokens[token_count][4]) << 10)
											/ 100;
								}
							} else if (strnicmp(line_tokens[token_count],
									"note=", 5) == 0) {
								if (!wm_isdigit(line_tokens[token_count][5])) {
									_WM_ERROR(__FUNCTION__, __LINE__,
											WM_ERR_INVALID_ARG,
											"(syntax error in patch line)", 0);
								} else {
									tmp_patch->note = atoi(
											&line_tokens[token_count][5]);
								}
							} else if (strnicmp(line_tokens[token_count],
									"env_time", 8) == 0) {
								if ((!wm_isdigit(line_tokens[token_count][8]))
										|| (!wm_isdigit(
												line_tokens[token_count][10]))
										|| (line_tokens[token_count][9] != '=')) {
									_WM_ERROR(__FUNCTION__, __LINE__,
											WM_ERR_INVALID_ARG,
											"(syntax error in patch line)", 0);
								} else {
									unsigned int env_no = atoi(
											&line_tokens[token_count][8]);
									if (env_no > 5) {
										_WM_ERROR(__FUNCTION__, __LINE__,
												WM_ERR_INVALID_ARG,
												"(syntax error in patch line)",
												0);
									} else {
										tmp_patch->env[env_no].time =
												(float) atof(
														&line_tokens[token_count][10]);
										if ((tmp_patch->env[env_no].time
												> 45000.0f)
												|| (tmp_patch->env[env_no].time
														< 1.47f)) {
											_WM_ERROR(__FUNCTION__, __LINE__,
													WM_ERR_INVALID_ARG,
													"(range error in patch line)",
													0);
											tmp_patch->env[env_no].set &= 0xFE;
										} else {
											tmp_patch->env[env_no].set |= 0x01;
										}
									}
								}
							} else if (strnicmp(line_tokens[token_count],
									"env_level", 9) == 0) {
								if ((!wm_isdigit(line_tokens[token_count][9]))
										|| (!wm_isdigit(
												line_tokens[token_count][11]))
										|| (line_tokens[token_count][10] != '=')) {
									_WM_ERROR(__FUNCTION__, __LINE__,
											WM_ERR_INVALID_ARG,
											"(syntax error in patch line)", 0);
								} else {
									unsigned int env_no = atoi(
											&line_tokens[token_count][9]);
									if (env_no > 5) {
										_WM_ERROR(__FUNCTION__, __LINE__,
												WM_ERR_INVALID_ARG,
												"(syntax error in patch line)",
												0);
									} else {
										tmp_patch->env[env_no].level =
												(float) atof(
														&line_tokens[token_count][11]);
										if ((tmp_patch->env[env_no].level > 1.0f)
												|| (tmp_patch->env[env_no].level
														< 0.0f)) {
											_WM_ERROR(__FUNCTION__, __LINE__,
													WM_ERR_INVALID_ARG,
													"(range error in patch line)",
													0);
											tmp_patch->env[env_no].set &= 0xFD;
										} else {
											tmp_patch->env[env_no].set |= 0x02;
										}
									}
								}
							} else if (stricmp(line_tokens[token_count],
									"keep=loop") == 0) {
								tmp_patch->keep |= SAMPLE_LOOP;
							} else if (stricmp(line_tokens[token_count],
									"keep=env") == 0) {
								tmp_patch->keep |= SAMPLE_ENVELOPE;
							} else if (stricmp(line_tokens[token_count],
									"remove=sustain") == 0) {
								tmp_patch->remove |= SAMPLE_SUSTAIN;
							} else if (stricmp(line_tokens[token_count],
									"remove=clamped") == 0) {
								tmp_patch->remove |= SAMPLE_CLAMPED;
							}
							token_count++;
						}
					}
				}
				/* free up tokens */
				free(line_tokens);
			}
			line_start_ptr = config_ptr + 1;
		}
		config_ptr++;
	}

	free(config_buffer);
	free(config_dir);

	return 0;
}

/* sample loading */

static int load_sample(struct _patch *sample_patch) {
	struct _sample *guspat = NULL;
	struct _sample *tmp_sample = NULL;
	unsigned int i = 0;

	/* we only want to try loading the guspat once. */
	sample_patch->loaded = 1;

	if ((guspat = _WM_load_gus_pat(sample_patch->filename, fix_release)) == NULL) {
		return -1;
	}

	if (auto_amp) {
		signed short int tmp_max = 0;
		signed short int tmp_min = 0;
		signed short samp_max = 0;
		signed short samp_min = 0;
		tmp_sample = guspat;
		do {
			samp_max = 0;
			samp_min = 0;
			for (i = 0; i < (tmp_sample->data_length >> 10); i++) {
				if (tmp_sample->data[i] > samp_max)
					samp_max = tmp_sample->data[i];
				if (tmp_sample->data[i] < samp_min)
					samp_min = tmp_sample->data[i];

			}
			if (samp_max > tmp_max)
				tmp_max = samp_max;
			if (samp_min < tmp_min)
				tmp_min = samp_min;
			tmp_sample = tmp_sample->next;
		} while (tmp_sample);
		if (auto_amp_with_amp) {
			if (tmp_max >= -tmp_min) {
				sample_patch->amp = (sample_patch->amp
						* ((32767 << 10) / tmp_max)) >> 10;
			} else {
				sample_patch->amp = (sample_patch->amp
						* ((32768 << 10) / -tmp_min)) >> 10;
			}
		} else {
			if (tmp_max >= -tmp_min) {
				sample_patch->amp = (32767 << 10) / tmp_max;
			} else {
				sample_patch->amp = (32768 << 10) / -tmp_min;
			}
		}
	}

	sample_patch->first_sample = guspat;

	if (sample_patch->patchid & 0x0080) {
		if (!(sample_patch->keep & SAMPLE_LOOP)) {
			do {
				guspat->modes &= 0xFB;
				guspat = guspat->next;
			} while (guspat);
		}
		guspat = sample_patch->first_sample;
		if (!(sample_patch->keep & SAMPLE_ENVELOPE)) {
			do {
				guspat->modes &= 0xBF;
				guspat = guspat->next;
			} while (guspat);
		}
		guspat = sample_patch->first_sample;
	}

	if (sample_patch->patchid == 47) {
		do {
			if (!(guspat->modes & SAMPLE_LOOP)) {
				for (i = 3; i < 6; i++) {
					guspat->env_target[i] = guspat->env_target[2];
					guspat->env_rate[i] = guspat->env_rate[2];
				}
			}
			guspat = guspat->next;
		} while (guspat);
		guspat = sample_patch->first_sample;
	}

	do {
		if ((sample_patch->remove & SAMPLE_SUSTAIN)
				&& (guspat->modes & SAMPLE_SUSTAIN)) {
			guspat->modes ^= SAMPLE_SUSTAIN;
		}
		if ((sample_patch->remove & SAMPLE_CLAMPED)
				&& (guspat->modes & SAMPLE_CLAMPED)) {
			guspat->modes ^= SAMPLE_CLAMPED;
		}
		if (sample_patch->keep & SAMPLE_ENVELOPE) {
			guspat->modes |= SAMPLE_ENVELOPE;
		}

		for (i = 0; i < 6; i++) {
			if (guspat->modes & SAMPLE_ENVELOPE) {
				if (sample_patch->env[i].set & 0x02) {
					guspat->env_target[i] = 16448
							* (signed long int) (255.0
									* sample_patch->env[i].level);
				}

				if (sample_patch->env[i].set & 0x01) {
					guspat->env_rate[i] = (signed long int) (4194303.0
							/ ((float) _WM_SampleRate
									* (sample_patch->env[i].time / 1000.0)));
				}
			} else {
				guspat->env_target[i] = 4194303;
				guspat->env_rate[i] = (signed long int) (4194303.0
						/ ((float) _WM_SampleRate * env_time_table[63]));
			}
		}

		guspat = guspat->next;
	} while (guspat);
	return 0;
}

static struct _patch *
get_patch_data(unsigned short patchid) {
	struct _patch *search_patch;

	_WM_Lock(&patch_lock);

	search_patch = patch[patchid & 0x007F];

	if (search_patch == NULL) {
		_WM_Unlock(&patch_lock);
		return NULL;
	}

	while (search_patch) {
		if (search_patch->patchid == patchid) {
			_WM_Unlock(&patch_lock);
			return search_patch;
		}
		search_patch = search_patch->next;
	}
	if ((patchid >> 8) != 0) {
		_WM_Unlock(&patch_lock);
		return (get_patch_data(patchid & 0x00FF));
	}
	_WM_Unlock(&patch_lock);
	return NULL;
}

static void load_patch(struct _mdi *mdi, unsigned short patchid) {
	unsigned int i;
	struct _patch *tmp_patch = NULL;

	for (i = 0; i < mdi->patch_count; i++) {
		if (mdi->patches[i]->patchid == patchid) {
			return;
		}
	}

	tmp_patch = get_patch_data(patchid);
	if (tmp_patch == NULL) {
		return;
	}

	_WM_Lock(&patch_lock);
	if (!tmp_patch->loaded) {
		if (load_sample(tmp_patch) == -1) {
			_WM_Unlock(&patch_lock);
			return;
		}
	}

	if (tmp_patch->first_sample == NULL) {
		_WM_Unlock(&patch_lock);
		return;
	}

	mdi->patch_count++;
	mdi->patches = (struct _patch**)realloc(mdi->patches,
			(sizeof(struct _patch*) * mdi->patch_count));
	mdi->patches[mdi->patch_count - 1] = tmp_patch;
	tmp_patch->inuse_count++;
	_WM_Unlock(&patch_lock);
}

static struct _sample *
get_sample_data(struct _patch *sample_patch, unsigned long int freq) {
	struct _sample *last_sample = NULL;
	struct _sample *return_sample = NULL;

	_WM_Lock(&patch_lock);
	if (sample_patch == NULL) {
		_WM_Unlock(&patch_lock);
		return NULL;
	}
	if (sample_patch->first_sample == NULL) {
		_WM_Unlock(&patch_lock);
		return NULL;
	}
	if (freq == 0) {
		_WM_Unlock(&patch_lock);
		return sample_patch->first_sample;
	}

	return_sample = sample_patch->first_sample;
	last_sample = sample_patch->first_sample;
	while (last_sample) {
		if (freq > last_sample->freq_low) {
			if (freq < last_sample->freq_high) {
				_WM_Unlock(&patch_lock);
				return last_sample;
			} else {
				return_sample = last_sample;
			}
		}
		last_sample = last_sample->next;
	}
	_WM_Unlock(&patch_lock);
	return return_sample;
}

static void do_note_off_extra(struct _note *nte) {

	nte->is_off = 0;

	if (nte->hold) {
		nte->hold |= HOLD_OFF;
	} else {
		if (!(nte->modes & SAMPLE_ENVELOPE)) {
			if (nte->modes & SAMPLE_LOOP) {
				nte->modes ^= SAMPLE_LOOP;
			}
			nte->env_inc = 0;

		} else if (nte->modes & SAMPLE_CLAMPED) {
			if (nte->env < 5) {
				nte->env = 5;
				if (nte->env_level > nte->sample->env_target[5]) {
					nte->env_inc = -nte->sample->env_rate[5];
				} else {
					nte->env_inc = nte->sample->env_rate[5];
				}
			}
#if 1
		} else if (nte->modes & SAMPLE_SUSTAIN) {
			if (nte->env < 3) {
				nte->env = 3;
				if (nte->env_level > nte->sample->env_target[3]) {
					nte->env_inc = -nte->sample->env_rate[3];
				} else {
					nte->env_inc = nte->sample->env_rate[3];
				}
			}
#endif
		} else if (nte->env < 4) {
			nte->env = 4;
			if (nte->env_level > nte->sample->env_target[4]) {
				nte->env_inc = -nte->sample->env_rate[4];
			} else {
				nte->env_inc = nte->sample->env_rate[4];
			}
		}
	}
}

static void do_note_off(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	nte = &mdi->note_table[0][ch][(data->data >> 8)];
	if (!nte->active)
		nte = &mdi->note_table[1][ch][(data->data >> 8)];
	if (!nte->active) {
		return;
	}

	if ((mdi->channel[ch].isdrum) && (!(nte->modes & SAMPLE_LOOP))) {
		return;
	}

	if (nte->env == 0) {
		nte->is_off = 1;
	} else {
		do_note_off_extra(nte);
	}
}

static inline unsigned long int get_inc(struct _mdi *mdi, struct _note *nte) {
	int ch = nte->noteid >> 8;
	signed long int note_f;
	unsigned long int freq;

	if (nte->patch->note != 0) {
		note_f = nte->patch->note * 100;
	} else {
		note_f = (nte->noteid & 0x7f) * 100;
	}
	note_f += mdi->channel[ch].pitch_adjust;
	if (note_f < 0) {
		note_f = 0;
	} else if (note_f > 12700) {
		note_f = 12700;
	}
	freq = freq_table[(note_f % 1200)] >> (10 - (note_f / 1200));
	return (((freq / ((_WM_SampleRate * 100) / 1024)) * 1024
			/ nte->sample->inc_div));
}

static inline unsigned long int get_volume(struct _mdi *mdi, unsigned char ch,
		struct _note *nte) {
	signed long int volume;

	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
		volume = (sqr_volume[mdi->channel[ch].volume]
				* sqr_volume[mdi->channel[ch].expression]
				* sqr_volume[nte->velocity]) / 1048576;
	} else {
		volume = (lin_volume[mdi->channel[ch].volume]
				* lin_volume[mdi->channel[ch].expression]
				* lin_volume[nte->velocity]) / 1048576;
	}

	volume = volume * nte->patch->amp / 100;
	return (volume);
}

static void do_note_on(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	struct _note *prev_nte;
	struct _note *nte_array;
	unsigned long int freq = 0;
	struct _patch *patch;
	struct _sample *sample;
	unsigned char ch = data->channel;
	unsigned char note = (unsigned char)(data->data >> 8);
	unsigned char velocity = (data->data & 0xFF);

	if (velocity == 0x00) {
		do_note_off(mdi, data);
		return;
	}

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	if (!mdi->channel[ch].isdrum) {
		patch = mdi->channel[ch].patch;
		if (patch == NULL) {
			return;
		}
		freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
	} else {
		patch = get_patch_data(((mdi->channel[ch].bank << 8) | note | 0x80));
		if (patch == NULL) {
			return;
		}
		if (patch->note) {
			freq = freq_table[(patch->note % 12) * 100]
					>> (10 - (patch->note / 12));
		} else {
			freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
		}
	}

	sample = get_sample_data(patch, (freq / 100));
	if (sample == NULL) {
		return;
	}

	nte = &mdi->note_table[0][ch][note];

	if (nte->active) {
		if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3)
				&& (!(nte->hold & HOLD_OFF)))
			return;
		nte->replay = &mdi->note_table[1][ch][note];
		nte->env = 6;
		nte->env_inc = -nte->sample->env_rate[6];
		nte = nte->replay;
	} else {
		if (mdi->note_table[1][ch][note].active) {
			if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3)
					&& (!(nte->hold & HOLD_OFF)))
				return;
			mdi->note_table[1][ch][note].replay = nte;
			mdi->note_table[1][ch][note].env = 6;
			mdi->note_table[1][ch][note].env_inc =
					-mdi->note_table[1][ch][note].sample->env_rate[6];
		} else {
			nte_array = mdi->note;
			if (nte_array == NULL) {
				mdi->note = nte;
			} else {
				do {
					prev_nte = nte_array;
					nte_array = nte_array->next;
				} while (nte_array);
				prev_nte->next = nte;
			}
			nte->active = 1;
			nte->next = NULL;
		}
	}
	nte->noteid = (ch << 8) | note;
	nte->patch = patch;
	nte->sample = sample;
	nte->sample_pos = 0;
	nte->sample_inc = get_inc(mdi, nte);
	nte->velocity = velocity;
	nte->env = 0;
	nte->env_inc = nte->sample->env_rate[0];
	nte->env_level = 0;
	nte->modes = sample->modes;
	nte->hold = mdi->channel[ch].hold;
	nte->vol_lvl = get_volume(mdi, ch, nte);
	nte->replay = NULL;
	nte->is_off = 0;
}

static void do_aftertouch(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	nte = &mdi->note_table[0][ch][(data->data >> 8)];
	if (!nte->active) {
		nte = &mdi->note_table[1][ch][(data->data >> 8)];
		if (!nte->active) {
			return;
		}
	}

	nte->velocity = data->data & 0xff;
	nte->vol_lvl = get_volume(mdi, ch, nte);

	if (nte->replay) {
		nte->replay->velocity = data->data & 0xff;
		nte->replay->vol_lvl = get_volume(mdi, ch, nte->replay);
	}
}

static void do_pan_adjust(struct _mdi *mdi, unsigned char ch) {
	signed short int pan_adjust = mdi->channel[ch].balance
			+ mdi->channel[ch].pan;
	signed short int left, right;
	int amp = 32;

	if (pan_adjust > 63) {
		pan_adjust = 63;
	} else if (pan_adjust < -64) {
		pan_adjust = -64;
	}

	pan_adjust += 64;
/*	if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {*/
	left = (pan_volume[127 - pan_adjust] * WM_MasterVolume * amp) / 1048576;
	right = (pan_volume[pan_adjust] * WM_MasterVolume * amp) / 1048576;
/*	} else {
	left = (lin_volume[127 - pan_adjust] * WM_MasterVolume * amp) / 1048576;
	right= (lin_volume[pan_adjust] * WM_MasterVolume * amp) / 1048576;
	}*/

	mdi->channel[ch].left_adjust = left;
	mdi->channel[ch].right_adjust = right;
}

static void do_control_bank_select(struct _mdi *mdi, struct _event_data *data) {
	unsigned char ch = data->channel;
	mdi->channel[ch].bank = (unsigned char)data->data;
}

static void do_control_data_entry_course(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;
	int data_tmp;

	if ((mdi->channel[ch].reg_non == 0)
			&& (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		data_tmp = mdi->channel[ch].pitch_range % 100;
		mdi->channel[ch].pitch_range = short(data->data * 100 + data_tmp);
	/*	printf("Data Entry Course: pitch_range: %i\n\r",mdi->channel[ch].pitch_range);*/
	/*	printf("Data Entry Course: data %li\n\r",data->data);*/
	}
}

static void do_control_channel_volume(struct _mdi *mdi,
		struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	mdi->channel[ch].volume = (unsigned char)data->data;

	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->vol_lvl = get_volume(mdi, ch, note_data);
				if (note_data->replay)
					note_data->replay->vol_lvl = get_volume(mdi, ch,
							note_data->replay);
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_control_channel_balance(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;

	mdi->channel[ch].balance = (signed char)(data->data - 64);
	do_pan_adjust(mdi, ch);
}

static void do_control_channel_pan(struct _mdi *mdi, struct _event_data *data) {
	unsigned char ch = data->channel;

	mdi->channel[ch].pan = (signed char)(data->data - 64);
	do_pan_adjust(mdi, ch);
}

static void do_control_channel_expression(struct _mdi *mdi,
		struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	mdi->channel[ch].expression = (unsigned char)data->data;

	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->vol_lvl = get_volume(mdi, ch, note_data);
				if (note_data->replay)
					note_data->replay->vol_lvl = get_volume(mdi, ch,
							note_data->replay);
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_control_data_entry_fine(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;
	int data_tmp;

	if ((mdi->channel[ch].reg_non == 0)
			&& (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		data_tmp = mdi->channel[ch].pitch_range / 100;
		mdi->channel[ch].pitch_range = short((data_tmp * 100) + data->data);
	/*	printf("Data Entry Fine: pitch_range: %i\n\r",mdi->channel[ch].pitch_range);*/
	/*	printf("Data Entry Fine: data: %li\n\r", data->data);*/
	}

}

static void do_control_channel_hold(struct _mdi *mdi, struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	if (data->data > 63) {
		mdi->channel[ch].hold = 1;
	} else {
		mdi->channel[ch].hold = 0;
		if (note_data) {
			do {
				if ((note_data->noteid >> 8) == ch) {
					if (note_data->hold & HOLD_OFF) {
						if (note_data->modes & SAMPLE_ENVELOPE) {
							if (note_data->modes & SAMPLE_CLAMPED) {
								if (note_data->env < 5) {
									note_data->env = 5;
									if (note_data->env_level
											> note_data->sample->env_target[5]) {
										note_data->env_inc =
												-note_data->sample->env_rate[5];
									} else {
										note_data->env_inc =
												note_data->sample->env_rate[5];
									}
								}
							} else if (note_data->env < 4) {
								note_data->env = 4;
								if (note_data->env_level
										> note_data->sample->env_target[4]) {
									note_data->env_inc =
											-note_data->sample->env_rate[4];
								} else {
									note_data->env_inc =
											note_data->sample->env_rate[4];
								}
							}
						} else {
							if (note_data->modes & SAMPLE_LOOP) {
								note_data->modes ^= SAMPLE_LOOP;
							}
							note_data->env_inc = 0;
						}
					}
					note_data->hold = 0x00;
				}
				note_data = note_data->next;
			} while (note_data);
		}
	}
}

static void do_control_data_increment(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;

	if ((mdi->channel[ch].reg_non == 0)
			&& (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		if (mdi->channel[ch].pitch_range < 0x3FFF)
			mdi->channel[ch].pitch_range++;
	}
}

static void do_control_data_decrement(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;

	if ((mdi->channel[ch].reg_non == 0)
			&& (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
		if (mdi->channel[ch].pitch_range > 0)
			mdi->channel[ch].pitch_range--;
	}
}
static void do_control_non_registered_param(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;
	mdi->channel[ch].reg_non = 1;
}

static void do_control_registered_param_fine(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;
	mdi->channel[ch].reg_data = (unsigned short) ((mdi->channel[ch].reg_data & 0x3F80)
			| data->data);
	mdi->channel[ch].reg_non = 0;
}

static void do_control_registered_param_course(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;
	mdi->channel[ch].reg_data = (unsigned short) ((mdi->channel[ch].reg_data & 0x7F)
			| (data->data << 7));
	mdi->channel[ch].reg_non = 0;
}

static void do_control_channel_sound_off(struct _mdi *mdi,
		struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->active = 0;
				if (note_data->replay) {
					note_data->replay = NULL;
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_control_channel_controllers_off(struct _mdi *mdi,
		struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	mdi->channel[ch].expression = 127;
	mdi->channel[ch].pressure = 127;
	mdi->channel[ch].volume = 100;
	mdi->channel[ch].pan = 0;
	mdi->channel[ch].balance = 0;
	mdi->channel[ch].reg_data = 0xffff;
	mdi->channel[ch].pitch_range = 200;
	mdi->channel[ch].pitch = 0;
	mdi->channel[ch].pitch_adjust = 0;
	mdi->channel[ch].hold = 0;
	do_pan_adjust(mdi, ch);

	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->sample_inc = get_inc(mdi, note_data);
				note_data->velocity = 0;
				note_data->vol_lvl = get_volume(mdi, ch, note_data);
				note_data->hold = 0;

				if (note_data->replay) {
					note_data->replay->velocity = (unsigned char)data->data;
					note_data->replay->vol_lvl = get_volume(mdi, ch,
							note_data->replay);
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_control_channel_notes_off(struct _mdi *mdi,
		struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	if (mdi->channel[ch].isdrum)
		return;
	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				if (!note_data->hold) {
					if (note_data->modes & SAMPLE_ENVELOPE) {
						if (note_data->env < 5) {
							if (note_data->env_level
									> note_data->sample->env_target[5]) {
								note_data->env_inc =
										-note_data->sample->env_rate[5];
							} else {
								note_data->env_inc =
										note_data->sample->env_rate[5];
							}
							note_data->env = 5;
						}
					}
				} else {
					note_data->hold |= HOLD_OFF;
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_patch(struct _mdi *mdi, struct _event_data *data) {
	unsigned char ch = data->channel;
	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	if (!mdi->channel[ch].isdrum) {
		mdi->channel[ch].patch = get_patch_data((unsigned short)(((mdi->channel[ch].bank << 8) | data->data)));
	} else {
		mdi->channel[ch].bank = (unsigned char)data->data;
	}
}

static void do_channel_pressure(struct _mdi *mdi, struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->velocity = (unsigned char)data->data;
				note_data->vol_lvl = get_volume(mdi, ch, note_data);

				if (note_data->replay) {
					note_data->replay->velocity = (unsigned char)data->data;
					note_data->replay->vol_lvl = get_volume(mdi, ch,
							note_data->replay);
				}
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_pitch(struct _mdi *mdi, struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);
	mdi->channel[ch].pitch = short(data->data - 0x2000);

	if (mdi->channel[ch].pitch < 0) {
		mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range
				* mdi->channel[ch].pitch / 8192;
	} else {
		mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range
				* mdi->channel[ch].pitch / 8191;
	}

	if (note_data) {
		do {
			if ((note_data->noteid >> 8) == ch) {
				note_data->sample_inc = get_inc(mdi, note_data);
			}
			note_data = note_data->next;
		} while (note_data);
	}
}

static void do_sysex_roland_drum_track(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;

	MIDI_EVENT_DEBUG(__FUNCTION__,ch);

	if (data->data > 0) {
		mdi->channel[ch].isdrum = 1;
		mdi->channel[ch].patch = NULL;
	} else {
		mdi->channel[ch].isdrum = 0;
		mdi->channel[ch].patch = get_patch_data(0);
	}
}

static void do_sysex_roland_reset(struct _mdi *mdi, struct _event_data *data) {
	int i;
	for (i = 0; i < 16; i++) {
		mdi->channel[i].bank = 0;
		if (i != 9) {
			mdi->channel[i].patch = get_patch_data(0);
		} else {
			mdi->channel[i].patch = NULL;
		}
		mdi->channel[i].hold = 0;
		mdi->channel[i].volume = 100;
		mdi->channel[i].pressure = 127;
		mdi->channel[i].expression = 127;
		mdi->channel[i].balance = 0;
		mdi->channel[i].pan = 0;
		mdi->channel[i].left_adjust = 1;
		mdi->channel[i].right_adjust = 1;
		mdi->channel[i].pitch = 0;
		mdi->channel[i].pitch_range = 200;
		mdi->channel[i].reg_data = 0xFFFF;
		mdi->channel[i].isdrum = 0;
		do_pan_adjust(mdi, i);
	}
	mdi->channel[9].isdrum = 1;
	UNUSED(data); /* NOOP, to please the compiler gods */
}

static void WM_ResetToStart(midi * handle) {
	struct _mdi *mdi = (struct _mdi *) handle;

	mdi->current_event = mdi->events;
	mdi->samples_to_mix = 0;
	mdi->info.current_sample = 0;

	do_sysex_roland_reset(mdi, NULL);
}

static int midi_setup_noteoff(struct _mdi *mdi, unsigned char channel,
		unsigned char note, unsigned char velocity) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_note_off;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = (note << 8)
				| velocity;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_note_off;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return 0;
}

static int midi_setup_noteon(struct _mdi *mdi, unsigned char channel,
		unsigned char note, unsigned char velocity) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_note_on;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = (note << 8)
				| velocity;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_note_on;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = (note << 8) | velocity;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}

	if (mdi->channel[channel].isdrum)
		load_patch(mdi, ((mdi->channel[channel].bank << 8) | (note | 0x80)));
	return 0;
}

static int midi_setup_aftertouch(struct _mdi *mdi, unsigned char channel,
		unsigned char note, unsigned char pressure) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_aftertouch;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = (note << 8)
				| pressure;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_aftertouch;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = (note << 8) | pressure;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return 0;
}

static int midi_setup_control(struct _mdi *mdi, unsigned char channel,
		unsigned char controller, unsigned char setting) {
	void (*tmp_event)(struct _mdi *mdi, struct _event_data *data) = NULL;

	switch (controller) {
	case 0:
		tmp_event = *do_control_bank_select;
		mdi->channel[channel].bank = setting;
		break;
	case 6:
		tmp_event = *do_control_data_entry_course;
		break;
	case 7:
		tmp_event = *do_control_channel_volume;
		mdi->channel[channel].volume = setting;
		break;
	case 8:
		tmp_event = *do_control_channel_balance;
		break;
	case 10:
		tmp_event = *do_control_channel_pan;
		break;
	case 11:
		tmp_event = *do_control_channel_expression;
		break;
	case 38:
		tmp_event = *do_control_data_entry_fine;
		break;
	case 64:
		tmp_event = *do_control_channel_hold;
		break;
	case 96:
		tmp_event = *do_control_data_increment;
		break;
	case 97:
		tmp_event = *do_control_data_decrement;
		break;
	case 98:
	case 99:
		tmp_event = *do_control_non_registered_param;
		break;
	case 100:
		tmp_event = *do_control_registered_param_fine;
		break;
	case 101:
		tmp_event = *do_control_registered_param_course;
		break;
	case 120:
		tmp_event = *do_control_channel_sound_off;
		break;
	case 121:
		tmp_event = *do_control_channel_controllers_off;
		break;
	case 123:
		tmp_event = *do_control_channel_notes_off;
		break;
	default:
		return 0;
	}
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = tmp_event;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = setting;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = tmp_event;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = setting;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return 0;
}

static int midi_setup_patch(struct _mdi *mdi, unsigned char channel,
		unsigned char patch) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_patch;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = patch;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_patch;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = patch;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	if (mdi->channel[channel].isdrum) {
		mdi->channel[channel].bank = patch;
	} else {
		load_patch(mdi, ((mdi->channel[channel].bank << 8) | patch));
		mdi->channel[channel].patch = get_patch_data(
				((mdi->channel[channel].bank << 8) | patch));
	}
	return 0;
}

static int midi_setup_channel_pressure(struct _mdi *mdi, unsigned char channel,
		unsigned char pressure) {

	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_channel_pressure;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = pressure;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_channel_pressure;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = pressure;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}

	return 0;
}

static int midi_setup_pitch(struct _mdi *mdi, unsigned char channel,
		unsigned short pitch) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_pitch;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = pitch;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_pitch;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = pitch;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return 0;
}

static int midi_setup_sysex_roland_drum_track(struct _mdi *mdi,
		unsigned char channel, unsigned short setting) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event =
				*do_sysex_roland_drum_track;
		mdi->events[mdi->event_count - 1].event_data.channel = channel;
		mdi->events[mdi->event_count - 1].event_data.data = setting;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_sysex_roland_drum_track;
		mdi->events[mdi->event_count].event_data.channel = channel;
		mdi->events[mdi->event_count].event_data.data = setting;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}

	if (setting > 0) {
		mdi->channel[channel].isdrum = 1;
	} else {
		mdi->channel[channel].isdrum = 0;
	}

	return 0;
}

static int midi_setup_sysex_roland_reset(struct _mdi *mdi) {
	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->events[mdi->event_count - 1].do_event = *do_sysex_roland_reset;
		mdi->events[mdi->event_count - 1].event_data.channel = 0;
		mdi->events[mdi->event_count - 1].event_data.data = 0;
	} else {
		WM_CheckEventMemoryPool(mdi);
		mdi->events[mdi->event_count].do_event = *do_sysex_roland_reset;
		mdi->events[mdi->event_count].event_data.channel = 0;
		mdi->events[mdi->event_count].event_data.data = 0;
		mdi->events[mdi->event_count].samples_to_next = 0;
		mdi->event_count++;
	}
	return 0;
}

static int add_handle(void * handle) {
	struct _hndl *tmp_handle = NULL;

	if (first_handle == NULL) {
		first_handle = (struct _hndl*)malloc(sizeof(struct _hndl));
		if (first_handle == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, " to get ram", errno);
			return -1;
		}
		first_handle->handle = handle;
		first_handle->prev = NULL;
		first_handle->next = NULL;
	} else {
		tmp_handle = first_handle;
		if (tmp_handle->next) {
			while (tmp_handle->next)
				tmp_handle = tmp_handle->next;
		}
		tmp_handle->next = (struct _hndl*)malloc(sizeof(struct _hndl));
		if (tmp_handle->next == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, " to get ram", errno);
			return -1;
		}
		tmp_handle->next->prev = tmp_handle;
		tmp_handle = tmp_handle->next;
		tmp_handle->next = NULL;
		tmp_handle->handle = handle;
	}
	return 0;
}

static struct _mdi *
Init_MDI(void) {
	struct _mdi *mdi;

	mdi = (struct _mdi*)malloc(sizeof(struct _mdi));
	memset(mdi, 0, (sizeof(struct _mdi)));

	mdi->info.copyright = NULL;
	mdi->info.mixer_options = WM_MixerOptions;

	load_patch(mdi, 0x0000);

	mdi->events_size = MEM_CHUNK;
	mdi->events = (struct _event*)malloc(mdi->events_size * sizeof(struct _event));
	mdi->events[0].do_event = NULL;
	mdi->events[0].event_data.channel = 0;
	mdi->events[0].event_data.data = 0;
	mdi->events[0].samples_to_next = 0;
	mdi->event_count++;

	mdi->current_event = mdi->events;
	mdi->samples_to_mix = 0;
	mdi->info.current_sample = 0;
	mdi->info.total_midi_time = 0;
	mdi->info.approx_total_samples = 0;

	do_sysex_roland_reset(mdi, NULL);

	return mdi;
}

static void freeMDI(struct _mdi *mdi) {
	struct _sample *tmp_sample;
	unsigned long int i;

	if (mdi->patch_count != 0) {
		_WM_Lock(&patch_lock);
		for (i = 0; i < mdi->patch_count; i++) {
			mdi->patches[i]->inuse_count--;
			if (mdi->patches[i]->inuse_count == 0) {
				/* free samples here */
				while (mdi->patches[i]->first_sample) {
					tmp_sample = mdi->patches[i]->first_sample->next;
					free(mdi->patches[i]->first_sample->data);
					free(mdi->patches[i]->first_sample);
					mdi->patches[i]->first_sample = tmp_sample;
				}
				mdi->patches[i]->loaded = 0;
			}
		}
		_WM_Unlock(&patch_lock);
		free(mdi->patches);
	}

	free(mdi->events);
	free(mdi->tmp_info);
	_WM_free_reverb(mdi->reverb);
	free(mdi->mix_buffer);
	free(mdi);
}

static unsigned long int get_decay_samples(struct _patch *patch, unsigned char note) {

	struct _sample *sample = NULL;
	unsigned long int freq = 0;
	unsigned long int decay_samples = 0;

	if (patch == NULL)
		return 0;

	/* first get the freq we need so we can check the right sample */
	if (patch->patchid & 0x80) {
		/* is a drum patch */
		if (patch->note) {
			freq = freq_table[(patch->note % 12) * 100]
					>> (10 - (patch->note / 12));
		} else {
			freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
		}
	} else {
		freq = freq_table[(note % 12) * 100] >> (10 - (note / 12));
	}

	/* get the sample */
	sample = get_sample_data(patch, (freq / 100));
	if (sample == NULL)
		return 0;

	if (patch->patchid & 0x80) {
		float sratedata = ((float) sample->rate / (float) _WM_SampleRate)
				* (float) (sample->data_length >> 10);
		decay_samples = (unsigned long int) sratedata;
	/*	printf("Drums (%i / %i) * %lu = %f\n", sample->rate, _WM_SampleRate, (sample->data_length >> 10), sratedata);*/
	} else if (sample->modes & SAMPLE_CLAMPED) {
		decay_samples = (4194303 / sample->env_rate[5]);
	/*	printf("clamped 4194303 / %lu = %lu\n", sample->env_rate[5], decay_samples);*/
	} else {
		decay_samples =
				((4194303 - sample->env_target[4]) / sample->env_rate[4])
						+ (sample->env_target[4] / sample->env_rate[5]);
	/*	printf("NOT clamped ((4194303 - %lu) / %lu) + (%lu / %lu)) = %lu\n", sample->env_target[4], sample->env_rate[4], sample->env_target[4], sample->env_rate[5], decay_samples);*/
	}
	return decay_samples;
}

static struct _mdi *
WM_ParseNewMidi(unsigned char *midi_data, unsigned int midi_size) {
	struct _mdi *mdi;
	unsigned int tmp_val;
	unsigned int midi_type;
	unsigned int track_size;
	unsigned char **tracks;
	unsigned int end_of_tracks = 0;
	unsigned int no_tracks;
	unsigned int i;
	unsigned int divisions = 96;
	unsigned int tempo = 500000;
	float samples_per_delta_f = 0.0;
	float microseconds_per_pulse = 0.0;
	float pulses_per_second = 0.0;

	unsigned long int sample_count = 0;
	float sample_count_tmp = 0;
	float sample_remainder = 0;
	unsigned char *sysex_store = NULL;
	unsigned long int sysex_store_len = 0;

	unsigned long int *track_delta;
	unsigned char *track_end;
	unsigned long int smallest_delta = 0;
	unsigned long int subtract_delta = 0;
	unsigned long int tmp_length = 0;
	unsigned char current_event = 0;
	unsigned char current_event_ch = 0;
	unsigned char *running_event;
	unsigned long int decay_samples = 0;

	if (memcmp(midi_data, "RIFF", 4) == 0) {
		midi_data += 20;
		midi_size -= 20;
	}
	if (memcmp(midi_data, "MThd", 4)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_MIDI, NULL, 0);
		return NULL;
	}
	midi_data += 4;
	midi_size -= 4;

	if (midi_size < 10) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
		return NULL;
	}

	/*
	 * Get Midi Header Size - must always be 6
	 */
	tmp_val = *midi_data++ << 24;
	tmp_val |= *midi_data++ << 16;
	tmp_val |= *midi_data++ << 8;
	tmp_val |= *midi_data++;
	midi_size -= 4;
	if (tmp_val != 6) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, NULL, 0);
		return NULL;
	}

	/*
	 * Get Midi Format - we only support 0, 1 & 2
	 */
	tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
	midi_size -= 2;
	if (tmp_val > 2) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, NULL, 0);
		return NULL;
	}
	midi_type = tmp_val;

	/*
	 * Get No. of Tracks
	 */
	tmp_val = *midi_data++ << 8;
	tmp_val |= *midi_data++;
	midi_size -= 2;
	if (tmp_val < 1) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(no tracks)", 0);
		return NULL;
	}
	no_tracks = tmp_val;

	/*
	 * Check that type 0 midi file has only 1 track
	 */
	if ((midi_type == 0) && (no_tracks > 1)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(expected 1 track for type 0 midi file, found more)", 0);
		return NULL;
	}

	/*
	 * Get Divisions
	 */
	divisions = *midi_data++ << 8;
	divisions |= *midi_data++;
	midi_size -= 2;
	if (divisions & 0x00008000) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, NULL, 0);
		return NULL;
	}

	if ((WM_MixerOptions & WM_MO_WHOLETEMPO)) {
		float bpm_f = (float) (60000000 / tempo);
		tempo = 60000000 / (unsigned long int) bpm_f;
	} else if ((WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
		float bpm_fr = (float) (60000000 / tempo) + 0.5f;
		tempo = 60000000 / (unsigned long int) bpm_fr;
	}
	/* Slow but needed for accuracy */
	microseconds_per_pulse = (float) tempo / (float) divisions;
	pulses_per_second = 1000000.0f / microseconds_per_pulse;
	samples_per_delta_f = (float) _WM_SampleRate / pulses_per_second;

	mdi = Init_MDI();

	tracks = (unsigned char**)malloc(sizeof(unsigned char *) * no_tracks);
	track_delta = (unsigned long*)malloc(sizeof(unsigned long int) * no_tracks);
	track_end = (unsigned char*)malloc(sizeof(unsigned char) * no_tracks);
	running_event = (unsigned char*)malloc(sizeof(unsigned char) * no_tracks);

	for (i = 0; i < no_tracks; i++) {
		if (midi_size < 8) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
			goto _end;
		}
		if (memcmp(midi_data, "MTrk", 4) != 0) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing track header)", 0);
			goto _end;
		}
		midi_data += 4;
		midi_size -= 4;

		track_size = *midi_data++ << 24;
		track_size |= *midi_data++ << 16;
		track_size |= *midi_data++ << 8;
		track_size |= *midi_data++;
		midi_size -= 4;
		if (midi_size < track_size) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
			goto _end;
		}
		if ((midi_data[track_size - 3] != 0xFF)
				|| (midi_data[track_size - 2] != 0x2F)
				|| (midi_data[track_size - 1] != 0x00)) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing EOT)", 0);
			goto _end;
		}
		tracks[i] = midi_data;
		midi_data += track_size;
		midi_size -= track_size;
		track_end[i] = 0;
		running_event[i] = 0;
		track_delta[i] = 0;
		decay_samples = 0;
		while (*tracks[i] > 0x7F) {
			track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
			tracks[i]++;
		}
		track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
		tracks[i]++;
	}

	/*
	 * Handle type 0 & 1 the same, but type 2 differently
	 */
	switch (midi_type) {
	case 0:
	case 1:
	/* Type 0 & 1 can use the same code */
	while (end_of_tracks != no_tracks) {
		smallest_delta = 0;
		for (i = 0; i < no_tracks; i++) {
			if (track_end[i])
				continue;

			if (track_delta[i]) {
				track_delta[i] -= subtract_delta;
				if (track_delta[i]) {
					if ((!smallest_delta)
							|| (smallest_delta > track_delta[i])) {
						smallest_delta = track_delta[i];
					}
					continue;
				}
			}
			do {
				if (*tracks[i] > 0x7F) {
					current_event = *tracks[i];
					tracks[i]++;
				} else {
					current_event = running_event[i];
					if (running_event[i] < 0x80) {
						_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing event)", 0);
						goto _end;
					}
				}
				current_event_ch = current_event & 0x0F;
				switch (current_event >> 4) {
				case 0x8:
					NOTEOFF: midi_setup_noteoff(mdi, current_event_ch,
							tracks[i][0], tracks[i][1]);
					/* To better calculate samples needed after the end of midi,
					 * we calculate samples for decay for note off */
					{
						unsigned long int tmp_decay_samples = 0;
						struct _patch *tmp_patch = NULL;
						if (mdi->channel[current_event_ch].isdrum) {
							tmp_patch = get_patch_data(
									((mdi->channel[current_event_ch].bank << 8)
											| tracks[i][0] | 0x80));
						/*	if (tmp_patch == NULL)
								printf("Drum patch not loaded 0x%02x on channel %i\n",((mdi->channel[current_event_ch].bank << 8) | tracks[i][0] | 0x80),current_event_ch);*/
						} else {
							tmp_patch = mdi->channel[current_event_ch].patch;
						/*	if (tmp_patch == NULL)
								printf("Channel %i patch not loaded\n", current_event_ch);*/
						}
						tmp_decay_samples = get_decay_samples(tmp_patch,
								tracks[i][0]);
						/* if the note off decay is more than the decay we currently tracking then
						 * we set it to new decay. */
						if (tmp_decay_samples > decay_samples) {
							decay_samples = tmp_decay_samples;
						}
					}

					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0x9:
					if (tracks[i][1] == 0) {
						goto NOTEOFF;
					}
					midi_setup_noteon(mdi, (current_event & 0x0F), tracks[i][0],
							tracks[i][1]);
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xA:
					midi_setup_aftertouch(mdi, (current_event & 0x0F),
							tracks[i][0], tracks[i][1]);
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xB:
					midi_setup_control(mdi, (current_event & 0x0F),
							tracks[i][0], tracks[i][1]);
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xC:
					midi_setup_patch(mdi, (current_event & 0x0F), *tracks[i]);
					tracks[i]++;
					running_event[i] = current_event;
					break;
				case 0xD:
					midi_setup_channel_pressure(mdi, (current_event & 0x0F),
							*tracks[i]);
					tracks[i]++;
					running_event[i] = current_event;
					break;
				case 0xE:
					midi_setup_pitch(mdi, (current_event & 0x0F),
							((tracks[i][1] << 7) | (tracks[i][0] & 0x7F)));
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xF: /* Meta Event */
					if (current_event == 0xFF) {
						if (tracks[i][0] == 0x02) { /* Copyright Event */
							/* Get Length */
							tmp_length = 0;
							tracks[i]++;
							while (*tracks[i] > 0x7f) {
								tmp_length = (tmp_length << 7)
										+ (*tracks[i] & 0x7f);
								tracks[i]++;
							}
							tmp_length = (tmp_length << 7)
									+ (*tracks[i] & 0x7f);
							/* Copy copyright info in the getinfo struct */
							if (mdi->info.copyright) {
								mdi->info.copyright = (char*)realloc(
										mdi->info.copyright,
										(strlen(mdi->info.copyright) + 1
												+ tmp_length + 1));
								strncpy(
										&mdi->info.copyright[strlen(
												mdi->info.copyright) + 1],
										(char *) tracks[i], tmp_length);
								mdi->info.copyright[strlen(mdi->info.copyright)
										+ 1 + tmp_length] = '\0';
								mdi->info.copyright[strlen(mdi->info.copyright)] = '\n';

							} else {
								mdi->info.copyright = (char*)malloc(tmp_length + 1);
								strncpy(mdi->info.copyright, (char *) tracks[i],
										tmp_length);
								mdi->info.copyright[tmp_length] = '\0';
							}
							tracks[i] += tmp_length + 1;
						} else if ((tracks[i][0] == 0x2F)
								&& (tracks[i][1] == 0x00)) {
							/* End of Track */
							end_of_tracks++;
							track_end[i] = 1;
							goto NEXT_TRACK;
						} else if ((tracks[i][0] == 0x51)
								&& (tracks[i][1] == 0x03)) {
							/* Tempo */
							tempo = (tracks[i][2] << 16) + (tracks[i][3] << 8)
									+ tracks[i][4];
							tracks[i] += 5;
							if (!tempo)
								tempo = 500000;

							if ((WM_MixerOptions & WM_MO_WHOLETEMPO)) {
								float bpm_f = (float) (60000000 / tempo);
								tempo = 60000000
										/ (unsigned long int) bpm_f;
							} else if ((WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
								float bpm_fr = (float) (60000000 / tempo)
										+ 0.5f;
								tempo = 60000000
										/ (unsigned long int) bpm_fr;
							}
							/* Slow but needed for accuracy */
							microseconds_per_pulse = (float) tempo
									/ (float) divisions;
							pulses_per_second = 1000000.0f
									/ microseconds_per_pulse;
							samples_per_delta_f = (float) _WM_SampleRate
									/ pulses_per_second;

						} else {
							tmp_length = 0;
							tracks[i]++;
							while (*tracks[i] > 0x7f) {
								tmp_length = (tmp_length << 7)
										+ (*tracks[i] & 0x7f);
								tracks[i]++;
							}
							tmp_length = (tmp_length << 7)
									+ (*tracks[i] & 0x7f);
							tracks[i] += tmp_length + 1;
						}
					} else if ((current_event == 0xF0)
							|| (current_event == 0xF7)) {
						/* Roland Sysex Events */
						unsigned long int sysex_len = 0;
						while (*tracks[i] > 0x7F) {
							sysex_len = (sysex_len << 7) + (*tracks[i] & 0x7F);
							tracks[i]++;
						}
						sysex_len = (sysex_len << 7) + (*tracks[i] & 0x7F);
						tracks[i]++;

						running_event[i] = 0;

						sysex_store = (unsigned char*)realloc(sysex_store,
								sizeof(unsigned char)
										* (sysex_store_len + sysex_len));
						memcpy(&sysex_store[sysex_store_len], tracks[i],
								sysex_len);
						sysex_store_len += sysex_len;

						if (sysex_store[sysex_store_len - 1] == 0xF7) {
							unsigned char tmpsysexdata[] = { 0x41, 0x10, 0x42, 0x12 };
							if (memcmp(tmpsysexdata, sysex_store, 4) == 0) {
								/* checksum */
								unsigned char sysex_cs = 0;
								unsigned int sysex_ofs = 4;
								do {
									sysex_cs += sysex_store[sysex_ofs];
									if (sysex_cs > 0x7F) {
										sysex_cs -= 0x80;
									}
									sysex_ofs++;
								} while (sysex_store[sysex_ofs + 1] != 0xF7);
								sysex_cs = 128 - sysex_cs;
								/* is roland sysex message valid */
								if (sysex_cs == sysex_store[sysex_ofs]) {
									/* process roland sysex event */
									if (sysex_store[4] == 0x40) {
										if (((sysex_store[5] & 0xF0) == 0x10)
												&& (sysex_store[6] == 0x15)) {
											/* Roland Drum Track Setting */
											unsigned char sysex_ch = 0x0F
													& sysex_store[5];
											if (sysex_ch == 0x00) {
												sysex_ch = 0x09;
											} else if (sysex_ch <= 0x09) {
												sysex_ch -= 1;
											}
											midi_setup_sysex_roland_drum_track(
													mdi, sysex_ch,
													sysex_store[7]);
										} else if ((sysex_store[5] == 0x00)
												&& (sysex_store[6] == 0x7F)
												&& (sysex_store[7] == 0x00)) {
											/* Roland GS Reset */
											midi_setup_sysex_roland_reset(mdi);
										}
									}
								}
							}
							free(sysex_store);
							sysex_store = NULL;
							sysex_store_len = 0;
						}
						tracks[i] += sysex_len;
					} else {
						_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(unrecognized meta event)", 0);
						goto _end;
					}
					break;
				default:
					_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(unrecognized event)", 0);
					goto _end;
				}
				while (*tracks[i] > 0x7F) {
					track_delta[i] = (track_delta[i] << 7)
							+ (*tracks[i] & 0x7F);
					tracks[i]++;
				}
				track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
				tracks[i]++;
			} while (!track_delta[i]);
			if ((!smallest_delta) || (smallest_delta > track_delta[i])) {
				smallest_delta = track_delta[i];
			}
			NEXT_TRACK: continue;
		}

		subtract_delta = smallest_delta;
		sample_count_tmp = (((float) smallest_delta * samples_per_delta_f)
				+ sample_remainder);
		sample_count = (unsigned long int) sample_count_tmp;
		sample_remainder = sample_count_tmp - (float) sample_count;
		if ((mdi->event_count)
				&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
			mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
		} else {
			WM_CheckEventMemoryPool(mdi);
			mdi->events[mdi->event_count].do_event = NULL;
			mdi->events[mdi->event_count].event_data.channel = 0;
			mdi->events[mdi->event_count].event_data.data = 0;
			mdi->events[mdi->event_count].samples_to_next = sample_count;
			mdi->event_count++;
		}
		mdi->info.approx_total_samples += sample_count;
		/* printf("Decay Samples = %lu\n",decay_samples);*/
		if (decay_samples > sample_count) {
			decay_samples -= sample_count;
		} else {
			decay_samples = 0;
		}
	}
	break;

	case 2: /* Type 2 has to be handled differently */
		for (i = 0; i < no_tracks; i++) {
			sample_remainder = 0.0;
			decay_samples = 0;
			track_delta[i] = 0;
			do {
				if(track_delta[i]) {
					sample_count_tmp = (((float) track_delta[i] * samples_per_delta_f)
							+ sample_remainder);
					sample_count = (unsigned long int) sample_count_tmp;
					sample_remainder = sample_count_tmp - (float) sample_count;
					if ((mdi->event_count)
							&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
						mdi->events[mdi->event_count - 1].samples_to_next += sample_count;
					} else {
						WM_CheckEventMemoryPool(mdi);
						mdi->events[mdi->event_count].do_event = NULL;
						mdi->events[mdi->event_count].event_data.channel = 0;
						mdi->events[mdi->event_count].event_data.data = 0;
						mdi->events[mdi->event_count].samples_to_next = sample_count;
						mdi->event_count++;
					}
					mdi->info.approx_total_samples += sample_count;
					/* printf("Decay Samples = %lu\n",decay_samples);*/
					if (decay_samples > sample_count) {
						decay_samples -= sample_count;
					} else {
						decay_samples = 0;
					}
				}
				if (*tracks[i] > 0x7F) {
					current_event = *tracks[i];
					tracks[i]++;
				} else {
					current_event = running_event[i];
					if (running_event[i] < 0x80) {
						_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing event)", 0);
						goto _end;
					}
				}
				current_event_ch = current_event & 0x0F;
				switch (current_event >> 4) {
				case 0x8:
					NOTEOFF2: midi_setup_noteoff(mdi, current_event_ch,
							tracks[i][0], tracks[i][1]);
					/* To better calculate samples needed after the end of midi,
					 * we calculate samples for decay for note off */
					{
						unsigned long int tmp_decay_samples = 0;
						struct _patch *tmp_patch = NULL;

						if (mdi->channel[current_event_ch].isdrum) {
							tmp_patch = get_patch_data(
									((mdi->channel[current_event_ch].bank << 8)
											| tracks[i][0] | 0x80));
						/*	if (tmp_patch == NULL)
								printf("Drum patch not loaded 0x%02x on channel %i\n",((mdi->channel[current_event_ch].bank << 8) | tracks[i][0] | 0x80),current_event_ch);*/
						} else {
							tmp_patch = mdi->channel[current_event_ch].patch;
						/*	if (tmp_patch == NULL)
								printf("Channel %i patch not loaded\n", current_event_ch);*/
						}
						tmp_decay_samples = get_decay_samples(tmp_patch,
								tracks[i][0]);
						/* if the note off decay is more than the decay we currently tracking then
						 * we set it to new decay. */
						if (tmp_decay_samples > decay_samples) {
							decay_samples = tmp_decay_samples;
						}
					}

					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0x9:
					if (tracks[i][1] == 0) {
						goto NOTEOFF2;
					}
					midi_setup_noteon(mdi, (current_event & 0x0F), tracks[i][0],
							tracks[i][1]);
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xA:
					midi_setup_aftertouch(mdi, (current_event & 0x0F),
							tracks[i][0], tracks[i][1]);
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xB:
					midi_setup_control(mdi, (current_event & 0x0F),
							tracks[i][0], tracks[i][1]);
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xC:
					midi_setup_patch(mdi, (current_event & 0x0F), *tracks[i]);
					tracks[i]++;
					running_event[i] = current_event;
					break;
				case 0xD:
					midi_setup_channel_pressure(mdi, (current_event & 0x0F),
							*tracks[i]);
					tracks[i]++;
					running_event[i] = current_event;
					break;
				case 0xE:
					midi_setup_pitch(mdi, (current_event & 0x0F),
							((tracks[i][1] << 7) | (tracks[i][0] & 0x7F)));
					tracks[i] += 2;
					running_event[i] = current_event;
					break;
				case 0xF: /* Meta Event */
					if (current_event == 0xFF) {
						if (tracks[i][0] == 0x02) { /* Copyright Event */
							/* Get Length */
							tmp_length = 0;
							tracks[i]++;
							while (*tracks[i] > 0x7f) {
								tmp_length = (tmp_length << 7)
										+ (*tracks[i] & 0x7f);
								tracks[i]++;
							}
							tmp_length = (tmp_length << 7)
									+ (*tracks[i] & 0x7f);
							/* Copy copyright info in the getinfo struct */
							if (mdi->info.copyright) {
								mdi->info.copyright = (char*)realloc(
										mdi->info.copyright,
										(strlen(mdi->info.copyright) + 1
												+ tmp_length + 1));
								strncpy(
										&mdi->info.copyright[strlen(
												mdi->info.copyright) + 1],
										(char *) tracks[i], tmp_length);
								mdi->info.copyright[strlen(mdi->info.copyright)
										+ 1 + tmp_length] = '\0';
								mdi->info.copyright[strlen(mdi->info.copyright)] = '\n';

							} else {
								mdi->info.copyright = (char*)malloc(tmp_length + 1);
								strncpy(mdi->info.copyright, (char *) tracks[i],
										tmp_length);
								mdi->info.copyright[tmp_length] = '\0';
							}
							tracks[i] += tmp_length + 1;
						} else if ((tracks[i][0] == 0x2F)
								&& (tracks[i][1] == 0x00)) {
							/* End of Track */
							end_of_tracks++;
							track_end[i] = 1;
							goto NEXT_TRACK2;
						} else if ((tracks[i][0] == 0x51)
								&& (tracks[i][1] == 0x03)) {
							/* Tempo */
							tempo = (tracks[i][2] << 16) + (tracks[i][3] << 8)
									+ tracks[i][4];
							tracks[i] += 5;
							if (!tempo)
								tempo = 500000;

							if ((WM_MixerOptions & WM_MO_WHOLETEMPO)) {
								float bpm_f = (float) (60000000 / tempo);
								tempo = 60000000
										/ (unsigned long int) bpm_f;
							} else if ((WM_MixerOptions & WM_MO_ROUNDTEMPO)) {
								float bpm_fr = (float) (60000000 / tempo)
										+ 0.5f;
								tempo = 60000000
										/ (unsigned long int) bpm_fr;
							}
							/* Slow but needed for accuracy */
							microseconds_per_pulse = (float) tempo
									/ (float) divisions;
							pulses_per_second = 1000000.0f
									/ microseconds_per_pulse;
							samples_per_delta_f = (float) _WM_SampleRate
									/ pulses_per_second;

						} else {
							tmp_length = 0;
							tracks[i]++;
							while (*tracks[i] > 0x7f) {
								tmp_length = (tmp_length << 7)
										+ (*tracks[i] & 0x7f);
								tracks[i]++;
							}
							tmp_length = (tmp_length << 7)
									+ (*tracks[i] & 0x7f);
							tracks[i] += tmp_length + 1;
						}
					} else if ((current_event == 0xF0)
							|| (current_event == 0xF7)) {
						/* Roland Sysex Events */
						unsigned long int sysex_len = 0;
						while (*tracks[i] > 0x7F) {
							sysex_len = (sysex_len << 7) + (*tracks[i] & 0x7F);
							tracks[i]++;
						}
						sysex_len = (sysex_len << 7) + (*tracks[i] & 0x7F);
						tracks[i]++;

						running_event[i] = 0;

						sysex_store = (unsigned char*)realloc(sysex_store,
								sizeof(unsigned char)
										* (sysex_store_len + sysex_len));
						memcpy(&sysex_store[sysex_store_len], tracks[i],
								sysex_len);
						sysex_store_len += sysex_len;

						if (sysex_store[sysex_store_len - 1] == 0xF7) {
							unsigned char tmpsysexdata[] = { 0x41, 0x10, 0x42, 0x12 };
							if (memcmp(tmpsysexdata, sysex_store, 4) == 0) {
								/* checksum */
								unsigned char sysex_cs = 0;
								unsigned int sysex_ofs = 4;
								do {
									sysex_cs += sysex_store[sysex_ofs];
									if (sysex_cs > 0x7F) {
										sysex_cs -= 0x80;
									}
									sysex_ofs++;
								} while (sysex_store[sysex_ofs + 1] != 0xF7);
								sysex_cs = 128 - sysex_cs;
								/* is roland sysex message valid */
								if (sysex_cs == sysex_store[sysex_ofs]) {
									/* process roland sysex event */
									if (sysex_store[4] == 0x40) {
										if (((sysex_store[5] & 0xF0) == 0x10)
												&& (sysex_store[6] == 0x15)) {
											/* Roland Drum Track Setting */
											unsigned char sysex_ch = 0x0F
													& sysex_store[5];
											if (sysex_ch == 0x00) {
												sysex_ch = 0x09;
											} else if (sysex_ch <= 0x09) {
												sysex_ch -= 1;
											}
											midi_setup_sysex_roland_drum_track(
													mdi, sysex_ch,
													sysex_store[7]);
										} else if ((sysex_store[5] == 0x00)
												&& (sysex_store[6] == 0x7F)
												&& (sysex_store[7] == 0x00)) {
											/* Roland GS Reset */
											midi_setup_sysex_roland_reset(mdi);
										}
									}
								}
							}
							free(sysex_store);
							sysex_store = NULL;
							sysex_store_len = 0;
						}
						tracks[i] += sysex_len;
					} else {
						_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(unrecognized meta event)", 0);
						goto _end;
					}
					break;
				default:
					_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(unrecognized event)", 0);
					goto _end;
				}
				track_delta[i] = 0;
				while (*tracks[i] > 0x7F) {
					track_delta[i] = (track_delta[i] << 7)
							+ (*tracks[i] & 0x7F);
					tracks[i]++;
				}
				track_delta[i] = (track_delta[i] << 7) + (*tracks[i] & 0x7F);
				tracks[i]++;
				NEXT_TRACK2:
				smallest_delta = track_delta[i]; /* Added just to keep Xcode happy */
				UNUSED(smallest_delta); /* Added to just keep clang happy */
			} while (track_end[i] == 0);
			/*
			 * Add decay at the end of each song
			 */
			if (decay_samples) {
				if ((mdi->event_count)
						&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
					mdi->events[mdi->event_count - 1].samples_to_next += decay_samples;
				} else {
					WM_CheckEventMemoryPool(mdi);
					mdi->events[mdi->event_count].do_event = NULL;
					mdi->events[mdi->event_count].event_data.channel = 0;
					mdi->events[mdi->event_count].event_data.data = 0;
					mdi->events[mdi->event_count].samples_to_next = decay_samples;
					mdi->event_count++;
				}
			}
		}
		break;

	default: break; /* Don't expect to get here, added for completeness */
	}

	if ((mdi->event_count)
			&& (mdi->events[mdi->event_count - 1].do_event == NULL)) {
		mdi->info.approx_total_samples -=
				mdi->events[mdi->event_count - 1].samples_to_next;
		mdi->event_count--;
	}
	/* Set total MIDI time to 1/1000's seconds */
	mdi->info.total_midi_time = (mdi->info.approx_total_samples * 1000)
			/ _WM_SampleRate;
	/*mdi->info.approx_total_samples += _WM_SampleRate * 3;*/

	/* Add additional samples needed for decay */
	mdi->info.approx_total_samples += decay_samples;
	/*printf("decay_samples = %lu\n",decay_samples);*/

	if ((mdi->reverb = _WM_init_reverb(_WM_SampleRate, reverb_room_width,
			reverb_room_length, reverb_listen_posx, reverb_listen_posy))
			== NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to init reverb", 0);
		goto _end;
	}

	mdi->info.current_sample = 0;
	mdi->current_event = &mdi->events[0];
	mdi->samples_to_mix = 0;
	mdi->note = NULL;

	WM_ResetToStart(mdi);

_end:	free(sysex_store);
	free(track_end);
	free(track_delta);
	free(running_event);
	free(tracks);
	if (mdi->reverb) return mdi;
	freeMDI(mdi);
	return NULL;
}

static int *WM_Mix_Linear(midi * handle, int * buffer, unsigned long int count)
{
	struct _mdi *mdi = (struct _mdi *)handle;
	unsigned long int data_pos;
	signed int premix, left_mix, right_mix;
	signed int vol_mul;
	struct _note *note_data = NULL;

	do {
		note_data = mdi->note;
		left_mix = right_mix = 0;
		if (note_data != NULL) {
			while (note_data) {
				/*
				 * ===================
				 * resample the sample
				 * ===================
				 */
				data_pos = note_data->sample_pos >> FPBITS;
				vol_mul = ((note_data->vol_lvl
						* (note_data->env_level >> 12)) >> FPBITS);

				premix = (note_data->sample->data[data_pos]
						+ ((note_data->sample->data[data_pos + 1]
								- note_data->sample->data[data_pos])
								* (signed long int) (note_data->sample_pos
										& FPMASK)>> FPBITS)) * vol_mul
						/ 1024;

				left_mix += premix
						* mdi->channel[note_data->noteid >> 8].left_adjust;
				right_mix += premix
						* mdi->channel[note_data->noteid >> 8].right_adjust;

				/*
				 * ========================
				 * sample position checking
				 * ========================
				 */
				note_data->sample_pos += note_data->sample_inc;
				if (note_data->sample_pos > note_data->sample->loop_end) {
					if (note_data->modes & SAMPLE_LOOP) {
						note_data->sample_pos =
								note_data->sample->loop_start
										+ ((note_data->sample_pos
												- note_data->sample->loop_start)
												% note_data->sample->loop_size);
					} else if (note_data->sample_pos >= note_data->sample->data_length) {
						if (note_data->replay == NULL) {
							goto KILL_NOTE;
						}
						goto RESTART_NOTE;
					}
				}

				if (note_data->env_inc == 0) {
					note_data = note_data->next;
					continue;
				}

				note_data->env_level += note_data->env_inc;
				if (note_data->env_level > 4194304) {
					note_data->env_level =
							note_data->sample->env_target[note_data->env];
				}
				if  (((note_data->env_inc < 0)
							&& (note_data->env_level
									> note_data->sample->env_target[note_data->env]))
					|| ((note_data->env_inc > 0)
							&& (note_data->env_level
									< note_data->sample->env_target[note_data->env])))
					{
					note_data = note_data->next;
					continue;
				}

				note_data->env_level =
						note_data->sample->env_target[note_data->env];
				switch (note_data->env) {
				case 0:
#if 0
					if (!(note_data->modes & SAMPLE_ENVELOPE)) {
						note_data->env_inc = 0;
						note_data = note_data->next;
						continue;
					}
#endif
					break;
				case 2:
					if (note_data->modes & SAMPLE_SUSTAIN) {
						note_data->env_inc = 0;
						note_data = note_data->next;
						continue;
					} else if (note_data->modes & SAMPLE_CLAMPED) {
						note_data->env = 5;
						if (note_data->env_level
								> note_data->sample->env_target[5]) {
							note_data->env_inc =
									-note_data->sample->env_rate[5];
						} else {
							note_data->env_inc =
									note_data->sample->env_rate[5];
						}
						continue;
					}
					break;
				case 5:
					if (note_data->env_level == 0) {
						goto KILL_NOTE;
					}
					/* sample release */
					if (note_data->modes & SAMPLE_LOOP)
						note_data->modes ^= SAMPLE_LOOP;
					note_data->env_inc = 0;
					note_data = note_data->next;
					continue;
				case 6:
					if (note_data->replay != NULL) {
						RESTART_NOTE: note_data->active = 0;
						{
							struct _note *prev_note = NULL;
							struct _note *nte_array = mdi->note;

							if (nte_array != note_data) {
								do {
									prev_note = nte_array;
									nte_array = nte_array->next;
								} while (nte_array != note_data);
							}
							if (prev_note) {
								prev_note->next = note_data->replay;
							} else {
								mdi->note = note_data->replay;
							}
							note_data->replay->next = note_data->next;
							note_data = note_data->replay;
							note_data->active = 1;
						}
					} else {
						KILL_NOTE: note_data->active = 0;
						{
							struct _note *prev_note = NULL;
							struct _note *nte_array = mdi->note;

							if (nte_array != note_data) {
								do {
									prev_note = nte_array;
									nte_array = nte_array->next;
								} while ((nte_array != note_data)
										&& (nte_array));
							}
							if (prev_note) {
								prev_note->next = note_data->next;
							} else {
								mdi->note = note_data->next;
							}
							note_data = note_data->next;
						}
					}
					continue;
				}
				note_data->env++;

				if (note_data->is_off == 1) {
					do_note_off_extra(note_data);
				}

				if (note_data->env_level
						> note_data->sample->env_target[note_data->env]) {
					note_data->env_inc =
							-note_data->sample->env_rate[note_data->env];
				} else {
					note_data->env_inc =
							note_data->sample->env_rate[note_data->env];
				}
				note_data = note_data->next;
				continue;
			}

			/*
			 * =========================
			 * mix the channels together
			 * =========================
			 */
			left_mix /= 1024;
			right_mix /= 1024;
		}

		*buffer++ = left_mix;
		*buffer++ = right_mix;
	} while (--count);
	return buffer;
}

static int *WM_Mix_Gauss(midi * handle, int * buffer, unsigned long int count)
{
	if (!gauss_table) init_gauss();

	struct _mdi *mdi = (struct _mdi *)handle;
	unsigned long int data_pos;
	signed int premix, left_mix, right_mix;
	signed int vol_mul;
	struct _note *note_data = NULL;
	signed short int *sptr;
	double y, xd;
	double *gptr, *gend;
	int left, right, temp_n;
	int ii, jj;

	do {
		note_data = mdi->note;
		left_mix = right_mix = 0;
		if (note_data != NULL) {
			while (note_data) {
				/*
				 * ===================
				 * resample the sample
				 * ===================
				 */
				data_pos = note_data->sample_pos >> FPBITS;
				vol_mul = ((note_data->vol_lvl
						* (note_data->env_level >> 12)) >> FPBITS);

				/* check to see if we're near one of the ends */
				left = data_pos;
				right = (note_data->sample->data_length >> FPBITS) - left
						- 1;
				temp_n = (right << 1) - 1;
				if (temp_n <= 0)
					temp_n = 1;
				if (temp_n > (left << 1) + 1)
					temp_n = (left << 1) + 1;

				/* use Newton if we can't fill the window */
				if (temp_n < gauss_n) {
					xd = note_data->sample_pos & FPMASK;
					xd /= (1L << FPBITS);
					xd += temp_n >> 1;
					y = 0;
					sptr = note_data->sample->data
							+ (note_data->sample_pos >> FPBITS)
							- (temp_n >> 1);
					for (ii = temp_n; ii;) {
						for (jj = 0; jj <= ii; jj++)
							y += sptr[jj] * newt_coeffs[ii][jj];
						y *= xd - --ii;
					}
					y += *sptr;
				} else { /* otherwise, use Gauss as usual */
					y = 0;
					gptr = &gauss_table[(note_data->sample_pos & FPMASK) *
							     (gauss_n + 1)];
					gend = gptr + gauss_n;
					sptr = note_data->sample->data
							+ (note_data->sample_pos >> FPBITS)
							- (gauss_n >> 1);
					do {
						y += *(sptr++) * *(gptr++);
					} while (gptr <= gend);
				}

				premix = (long) (y * vol_mul / 1024);

				left_mix += premix
						* mdi->channel[note_data->noteid >> 8].left_adjust;
				right_mix += premix
						* mdi->channel[note_data->noteid >> 8].right_adjust;

				/*
				 * ========================
				 * sample position checking
				 * ========================
				 */
				note_data->sample_pos += note_data->sample_inc;
				if (note_data->sample_pos > note_data->sample->loop_end)
				{
					if (note_data->modes & SAMPLE_LOOP) {
						note_data->sample_pos =
								note_data->sample->loop_start
										+ ((note_data->sample_pos
												- note_data->sample->loop_start)
												% note_data->sample->loop_size);
					} else if (note_data->sample_pos >= note_data->sample->data_length) {
						if (note_data->replay == NULL) {
							goto KILL_NOTE;
						}
						goto RESTART_NOTE;
					}
				}

				if (note_data->env_inc == 0) {
					note_data = note_data->next;
					continue;
				}

				note_data->env_level += note_data->env_inc;
				if (note_data->env_level > 4194304) {
					note_data->env_level =
							note_data->sample->env_target[note_data->env];
				}
				if (
						((note_data->env_inc < 0)
								&& (note_data->env_level
										> note_data->sample->env_target[note_data->env]))
						|| ((note_data->env_inc > 0)
								&& (note_data->env_level
										< note_data->sample->env_target[note_data->env]))
						) {
					note_data = note_data->next;
					continue;
				}

				note_data->env_level =
						note_data->sample->env_target[note_data->env];
				switch (note_data->env) {
				case 0:
#if 0
					if (!(note_data->modes & SAMPLE_ENVELOPE)) {
						note_data->env_inc = 0;
						note_data = note_data->next;
						continue;
					}
#endif
					break;
				case 2:
					if (note_data->modes & SAMPLE_SUSTAIN) {
						note_data->env_inc = 0;
						note_data = note_data->next;
						continue;
					} else if (note_data->modes & SAMPLE_CLAMPED) {
						note_data->env = 5;
						if (note_data->env_level
								> note_data->sample->env_target[5]) {
							note_data->env_inc =
									-note_data->sample->env_rate[5];
						} else {
							note_data->env_inc =
									note_data->sample->env_rate[5];
						}
						continue;
					}
					break;
				case 5:
					if (note_data->env_level == 0) {
						goto KILL_NOTE;
					}
					/* sample release */
					if (note_data->modes & SAMPLE_LOOP)
						note_data->modes ^= SAMPLE_LOOP;
					note_data->env_inc = 0;
					note_data = note_data->next;
					continue;
				case 6:
					if (note_data->replay != NULL) {
						RESTART_NOTE: note_data->active = 0;
						{
							struct _note *prev_note = NULL;
							struct _note *nte_array = mdi->note;

							if (nte_array != note_data) {
								do {
									prev_note = nte_array;
									nte_array = nte_array->next;
								} while (nte_array != note_data);
							}
							if (prev_note) {
								prev_note->next = note_data->replay;
							} else {
								mdi->note = note_data->replay;
							}
							note_data->replay->next = note_data->next;
							note_data = note_data->replay;
							note_data->active = 1;
						}
					} else {
						KILL_NOTE: note_data->active = 0;
						{
							struct _note *prev_note = NULL;
							struct _note *nte_array = mdi->note;

							if (nte_array != note_data) {
								do {
									prev_note = nte_array;
									nte_array = nte_array->next;
								} while ((nte_array != note_data)
										&& (nte_array));
							}
							if (prev_note) {
								prev_note->next = note_data->next;
							} else {
								mdi->note = note_data->next;
							}
							note_data = note_data->next;
						}
					}
					continue;
				}
				note_data->env++;

				if (note_data->is_off == 1) {
					do_note_off_extra(note_data);
				}

				if (note_data->env_level
						> note_data->sample->env_target[note_data->env]) {
					note_data->env_inc =
							-note_data->sample->env_rate[note_data->env];
				} else {
					note_data->env_inc =
							note_data->sample->env_rate[note_data->env];
				}
				note_data = note_data->next;
				continue;
			}

			/*
			 * =========================
			 * mix the channels together
			 * =========================
			 */
			left_mix /= 1024;
			right_mix /= 1024;
		}

		*buffer++ = left_mix;
		*buffer++ = right_mix;
	} while (--count);
	return buffer;
}

int *WM_Mix(midi *handle, int *buffer, unsigned long count)
{
	if (((struct _mdi *)handle)->info.mixer_options & WM_MO_ENHANCED_RESAMPLING)
	{
		return WM_Mix_Gauss(handle, buffer, count);
	}
	else
	{
		return WM_Mix_Linear(handle, buffer, count);
	}
}

static int WM_DoGetOutput(midi * handle, char * buffer,
		unsigned long int size) {
	unsigned long int buffer_used = 0;
	unsigned long int i;
	struct _mdi *mdi = (struct _mdi *) handle;
	unsigned long int real_samples_to_mix = 0;
	struct _event *event = mdi->current_event;
	signed int *tmp_buffer;
	signed int *out_buffer;
	signed int left_mix, right_mix;

	_WM_Lock(&mdi->lock);

	buffer_used = 0;
	memset(buffer, 0, size);
	if ( (size / 2) > mdi->mix_buffer_size) {
		if ( (size / 2) <= ( mdi->mix_buffer_size * 2 )) {
			mdi->mix_buffer_size += MEM_CHUNK;
		} else {
			mdi->mix_buffer_size = size / 2;
		}
		mdi->mix_buffer = (int*)realloc(mdi->mix_buffer, mdi->mix_buffer_size * sizeof(signed int));
	}
	tmp_buffer = mdi->mix_buffer;
	memset(tmp_buffer, 0, ((size / 2) * sizeof(signed long int)));
	out_buffer = tmp_buffer;

	do {
		if (!mdi->samples_to_mix) {
			while ((!mdi->samples_to_mix) && (event->do_event)) {
				event->do_event(mdi, &event->event_data);
				event++;
				mdi->samples_to_mix = event->samples_to_next;
				mdi->current_event = event;
			}

			if (!mdi->samples_to_mix) {
				if (mdi->info.current_sample
						>= mdi->info.approx_total_samples) {
					break;
				} else if ((mdi->info.approx_total_samples
						- mdi->info.current_sample) > (size >> 2)) {
					mdi->samples_to_mix = size >> 2;
				} else {
					mdi->samples_to_mix = mdi->info.approx_total_samples
							- mdi->info.current_sample;
				}
			}
		}
		if (mdi->samples_to_mix > (size >> 2)) {
			real_samples_to_mix = size >> 2;
		} else {
			real_samples_to_mix = mdi->samples_to_mix;
			if (real_samples_to_mix == 0) {
				continue;
			}
		}

		/* do mixing here */
		tmp_buffer = WM_Mix(handle, tmp_buffer, real_samples_to_mix);

		buffer_used += real_samples_to_mix * 4;
		size -= (real_samples_to_mix << 2);
		mdi->info.current_sample += real_samples_to_mix;
		mdi->samples_to_mix -= real_samples_to_mix;
	} while (size);

	tmp_buffer = out_buffer;

	if (mdi->info.mixer_options & WM_MO_REVERB) {
		_WM_do_reverb(mdi->reverb, tmp_buffer, (buffer_used / 2));
	}

	for (i = 0; i < buffer_used; i += 4) {
		left_mix = *tmp_buffer++;
		right_mix = *tmp_buffer++;

		if (left_mix > 32767) {
			left_mix = 32767;
		} else if (left_mix < -32768) {
			left_mix = -32768;
		}

		if (right_mix > 32767) {
			right_mix = 32767;
		} else if (right_mix < -32768) {
			right_mix = -32768;
		}

		/*
		 * ===================
		 * Write to the buffer
		 * ===================
		 */
		((short *)buffer)[0] = (short)left_mix;
		((short *)buffer)[1] = (short)right_mix;
		buffer += 4;
	}
	_WM_Unlock(&mdi->lock);
	return buffer_used;
}

/*
 * =========================
 * External Functions
 * =========================
 */

WM_SYMBOL const char *
WildMidi_GetString(unsigned short int info) {
	switch (info) {
	case WM_GS_VERSION:
		return WM_Version;
	}
	return NULL;
}

WM_SYMBOL int WildMidi_Init(const char * config_file, unsigned short int rate,
		unsigned short int options) {
	if (WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_ALR_INIT, NULL, 0);
		return -1;
	}

	if (config_file == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(NULL config file pointer)", 0);
		return -1;
	}
	WM_InitPatches();
	if (WM_LoadConfig(config_file) == -1) {
		return -1;
	}

	if (options & 0x5FF8) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)",
				0);
		WM_FreePatches();
		return -1;
	}
	WM_MixerOptions = options;

	if (rate < 11025) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(rate out of bounds, range is 11025 - 65535)", 0);
		WM_FreePatches();
		return -1;
	}
	_WM_SampleRate = rate;

	gauss_lock = 0;
	patch_lock = 0;
	WM_Initialized = 1;

	return 0;
}

WM_SYMBOL int WildMidi_GetSampleRate(void)
{
	return _WM_SampleRate;
}

WM_SYMBOL int WildMidi_MasterVolume(unsigned char master_volume) {
	struct _mdi *mdi = NULL;
	struct _hndl * tmp_handle = first_handle;
	int i = 0;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (master_volume > 127) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(master volume out of range, range is 0-127)", 0);
		return -1;
	}

	WM_MasterVolume = lin_volume[master_volume];

	if (tmp_handle) {
		while (tmp_handle) {
			mdi = (struct _mdi *) tmp_handle->handle;
			for (i = 0; i < 16; i++) {
				do_pan_adjust(mdi, i);
			}
			tmp_handle = tmp_handle->next;
		}
	}

	return 0;
}

WM_SYMBOL int WildMidi_Close(midi * handle) {
	struct _mdi *mdi = (struct _mdi *) handle;
	struct _hndl * tmp_handle;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)",
				0);
		return -1;
	}
	if (first_handle == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(no midi's open)",
				0);
		return -1;
	}
	_WM_Lock(&mdi->lock);
	if (first_handle->handle == handle) {
		tmp_handle = first_handle->next;
		free(first_handle);
		first_handle = tmp_handle;
		if (first_handle)
			first_handle->prev = NULL;
	} else {
		tmp_handle = first_handle;
		while (tmp_handle->handle != handle) {
			tmp_handle = tmp_handle->next;
			if (tmp_handle == NULL) {
				break;
			}
		}
		if (tmp_handle) {
			tmp_handle->prev->next = tmp_handle->next;
			if (tmp_handle->next) {
				tmp_handle->next->prev = tmp_handle->prev;
			}
			free(tmp_handle);
		}
	}

	freeMDI(mdi);

	return 0;
}

WM_SYMBOL midi *
WildMidi_Open(const char *midifile) {
	unsigned char *mididata = NULL;
	unsigned long int midisize = 0;
	midi * ret = NULL;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (midifile == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL filename)",
				0);
		return NULL;
	}

	if ((mididata = _WM_BufferFile(midifile, &midisize)) == NULL) {
		return NULL;
	}

	ret = (void *) WM_ParseNewMidi(mididata, midisize);
	free(mididata);

	if (ret) {
		if (add_handle(ret) != 0) {
			WildMidi_Close(ret);
			ret = NULL;
		}
	}

	return ret;
}

WM_SYMBOL midi *
WildMidi_OpenBuffer(unsigned char *midibuffer, unsigned long int size) {
	midi * ret = NULL;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (midibuffer == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(NULL midi data buffer)", 0);
		return NULL;
	}
	if (size > WM_MAXFILESIZE) {
		/* don't bother loading suspiciously long files */
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LONGFIL, NULL, 0);
		return NULL;
	}
	ret = (void *) WM_ParseNewMidi(midibuffer, size);

	if (ret) {
		if (add_handle(ret) != 0) {
			WildMidi_Close(ret);
			ret = NULL;
		}
	}

	return ret;
}

midi *WildMidi_NewMidi() {
	midi * ret = NULL;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	ret = Init_MDI();
	if (ret) {
		if (add_handle(ret) != 0) {
			WildMidi_Close(ret);
			ret = NULL;
		}
	}
	return ret;
}

WM_SYMBOL int WildMidi_FastSeek(midi * handle, unsigned long int *sample_pos) {
	struct _mdi *mdi;
	struct _event *event;
	struct _note *note_data;
	unsigned long int real_samples_to_mix;
	unsigned long int count;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)",
				0);
		return -1;
	}
	if (sample_pos == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(NULL seek position pointer)", 0);
		return -1;
	}

	mdi = (struct _mdi *) handle;
	_WM_Lock(&mdi->lock);
	event = mdi->current_event;

	/* make sure we havent asked for a positions beyond the end of the song. */
	if (*sample_pos > mdi->info.approx_total_samples) {
		/* if so set the position to the end of the song */
		*sample_pos = mdi->info.approx_total_samples;
	}

	/* was end of song requested and are we are there? */
	if (*sample_pos == mdi->info.current_sample) {
		/* yes */
		_WM_Unlock(&mdi->lock);
		return 0;
	}

	/* did we want to fast forward? */
	if (mdi->info.current_sample < *sample_pos) {
		/* yes */
		count = *sample_pos - mdi->info.current_sample;
	} else {
		/* no, reset values to start as the beginning */
		count = *sample_pos;
		WM_ResetToStart(handle);
		event = mdi->current_event;
	}

	/* clear the reverb buffers since we not gonna be using them here */
	_WM_reset_reverb(mdi->reverb);

	while (count) {
		if (!mdi->samples_to_mix) {
			while ((!mdi->samples_to_mix) && (event->do_event)) {
				event->do_event(mdi, &event->event_data);
				event++;
				mdi->samples_to_mix = event->samples_to_next;
				mdi->current_event = event;
			}

			if (!mdi->samples_to_mix) {
				if (event->do_event == NULL) {
					mdi->samples_to_mix = mdi->info.approx_total_samples
							- *sample_pos;
				} else {
					mdi->samples_to_mix = count;
				}
			}
		}

		if (mdi->samples_to_mix > count) {
			real_samples_to_mix = count;
		} else {
			real_samples_to_mix = mdi->samples_to_mix;
		}

		if (real_samples_to_mix == 0) {
			break;
		}

		count -= real_samples_to_mix;
		mdi->info.current_sample += real_samples_to_mix;
		mdi->samples_to_mix -= real_samples_to_mix;
	}

	note_data = mdi->note;
	if (note_data) {
		do {
			note_data->active = 0;
			if (note_data->replay) {
				note_data->replay = NULL;
			}
			note_data = note_data->next;
		} while (note_data);
	}
	mdi->note = NULL;

	_WM_Unlock(&mdi->lock);
	return 0;
}

WM_SYMBOL int WildMidi_GetOutput(midi * handle, char *buffer, unsigned long int size) {
	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)",
				0);
		return -1;
	}
	if (buffer == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(NULL buffer pointer)", 0);
		return -1;
	}
	if (size == 0) {
		return 0;
	}
	if (!!(size % 4)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(size not a multiple of 4)", 0);
		return -1;
	}
	return WM_DoGetOutput(handle, buffer, size);
}

WM_SYMBOL int WildMidi_SetOption(midi * handle, unsigned short int options,
		unsigned short int setting) {
	struct _mdi *mdi;
	int i;

	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return -1;
	}
	if (handle == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)",
				0);
		return -1;
	}

	mdi = (struct _mdi *) handle;
	_WM_Lock(&mdi->lock);
	if ((!(options & 0x0007)) || (options & 0xFFF8)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)",
				0);
		_WM_Unlock(&mdi->lock);
		return -1;
	}
	if (setting & 0xFFF8) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(invalid setting)", 0);
		_WM_Unlock(&mdi->lock);
		return -1;
	}

	mdi->info.mixer_options = ((mdi->info.mixer_options & (0x00FF ^ options))
			| (options & setting));

	if (options & WM_MO_LOG_VOLUME) {
		struct _note *note_data = mdi->note;

		for (i = 0; i < 16; i++) {
			do_pan_adjust(mdi, i);
		}

		if (note_data) {
			do {
				note_data->vol_lvl = get_volume(mdi, (note_data->noteid >> 8),
						note_data);
				if (note_data->replay)
					note_data->replay->vol_lvl = get_volume(mdi,
							(note_data->noteid >> 8), note_data->replay);
				note_data = note_data->next;
			} while (note_data);
		}
	} else if (options & WM_MO_REVERB) {
		_WM_reset_reverb(mdi->reverb);
	}

	_WM_Unlock(&mdi->lock);
	return 0;
}

WM_SYMBOL struct _WM_Info *
WildMidi_GetInfo(midi * handle) {
	struct _mdi *mdi = (struct _mdi *) handle;
	if (!WM_Initialized) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_NOT_INIT, NULL, 0);
		return NULL;
	}
	if (handle == NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(NULL handle)",
				0);
		return NULL;
	}
	_WM_Lock(&mdi->lock);
	if (mdi->tmp_info == NULL) {
		mdi->tmp_info = (struct _WM_Info*)malloc(sizeof(struct _WM_Info));
		if (mdi->tmp_info == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to set info", 0);
			_WM_Unlock(&mdi->lock);
			return NULL;
		}
		mdi->tmp_info->copyright = NULL;
	}
	mdi->tmp_info->current_sample = mdi->info.current_sample;
	mdi->tmp_info->approx_total_samples = mdi->info.approx_total_samples;
	mdi->tmp_info->mixer_options = mdi->info.mixer_options;
	if (mdi->info.copyright) {
		free(mdi->tmp_info->copyright);
		mdi->tmp_info->copyright = (char*)malloc(strlen(mdi->info.copyright) + 1);
		strcpy(mdi->tmp_info->copyright, mdi->info.copyright);
	} else {
		mdi->tmp_info->copyright = NULL;
	}
	_WM_Unlock(&mdi->lock);
	return mdi->tmp_info;
}

WM_SYMBOL int WildMidi_Shutdown(void) {
	if (!WM_Initialized) {
		// No error if trying to shut down an uninitialized device.
		return 0;
	}
	while (first_handle) {
		/* closes open handle and rotates the handles list. */
		WildMidi_Close((struct _mdi *) first_handle->handle);
	}
	WM_FreePatches();
	free_gauss();

	/* reset the globals */
	WM_MasterVolume = 948;
	WM_MixerOptions = 0;
	fix_release = 0;
	auto_amp = 0;
	auto_amp_with_amp = 0;
	reverb_room_width = 16.875f;
	reverb_room_length = 22.5f;
	reverb_listen_posx = 8.4375f;
	reverb_listen_posy = 16.875f;

	WM_Initialized = 0;

	return 0;
}

WildMidi_Renderer::WildMidi_Renderer()
{
	handle = WildMidi_NewMidi();
}

WildMidi_Renderer::~WildMidi_Renderer()
{
	WildMidi_Close((midi *)handle);
}

void WildMidi_Renderer::ShortEvent(int status, int parm1, int parm2)
{
	_mdi *mdi = (_mdi *)handle;
	_event_data ev;

	ev.channel = status & 0x0F;
	switch ((status & 0xF0) >> 4)	// command
	{
	case 0x8:
		ev.data = (parm1 << 8) | parm2;
		do_note_off(mdi, &ev);
		break;

	case 0x9:
		ev.data = (parm1 << 8) | parm2;
		do_note_on(mdi, &ev);
		break;

	case 0xA:
		ev.data = (parm1 << 8) | parm2;
		do_aftertouch(mdi, &ev);
		break;

	case 0xC:
		ev.data = parm1;
		do_patch(mdi, &ev);
		break;

	case 0xD:
		ev.data = parm1;
		do_channel_pressure(mdi, &ev);
		break;

	case 0xE:
		ev.data = parm1 | (parm2 << 7);
		do_pitch(mdi, &ev);
		break;

	case 0xB:	// Controllers
		ev.data = parm2;
		switch (parm1)
		{
		case 0:		do_control_bank_select(mdi, &ev);				break;
		case 6:		do_control_data_entry_course(mdi, &ev);			break;	// [sic]
		case 7:		do_control_channel_volume(mdi, &ev);			break;
		case 8:		do_control_channel_balance(mdi, &ev);			break;
		case 10:	do_control_channel_pan(mdi, &ev);				break;
		case 11:	do_control_channel_expression(mdi, &ev);		break;
		case 38:	do_control_data_entry_fine(mdi, &ev);			break;
		case 64:	do_control_channel_hold(mdi, &ev);				break;
		case 96:	do_control_data_increment(mdi, &ev);			break;
		case 97:	do_control_data_decrement(mdi, &ev);			break;
		case 98:
		case 99:	do_control_non_registered_param(mdi, &ev);		break;
		case 100:	do_control_registered_param_fine(mdi, &ev);		break;
		case 101:	do_control_registered_param_course(mdi, &ev);	break;	// [sic]
		case 120:	do_control_channel_sound_off(mdi, &ev);			break;
		case 121:	do_control_channel_controllers_off(mdi, &ev);	break;
		case 123:	do_control_channel_notes_off(mdi, &ev);			break;
		}
	}
}

void WildMidi_Renderer::LongEvent(const char *data, int len)
{
}

void WildMidi_Renderer::ComputeOutput(float *fbuffer, int len)
{
	_mdi *mdi = (_mdi *)handle;
	int *buffer = (int *)fbuffer;
	int *newbuf = WM_Mix(handle, buffer, len);
//	assert(newbuf - buffer == len);
	if (mdi->info.mixer_options & WM_MO_REVERB) {
		_WM_do_reverb(mdi->reverb, buffer, len * 2);
	}
	for (; buffer < newbuf; ++buffer)
	{
		*(float *)buffer = (float)*buffer / 32768.f;
	}
}

void WildMidi_Renderer::LoadInstrument(int bank, int percussion, int instr)
{
	load_patch((_mdi *)handle, (bank << 8) | instr | (percussion ? 0x80 : 0));
}

int WildMidi_Renderer::GetVoiceCount()
{
	int count = 0;
	for (_note *note_data = ((_mdi *)handle)->note; note_data != NULL; note_data = note_data->next)
	{
		count++;
	}
	return count;
}
