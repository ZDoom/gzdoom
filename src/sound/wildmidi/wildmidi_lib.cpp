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
#include <strings.h>
#endif
#include <stdlib.h>
#include <memory>
#include <mutex>

#include "common.h"
#include "wm_error.h"
#include "file_io.h"
#include "reverb.h"
#include "gus_pat.h"
#include "wildmidi_lib.h"
#include "files.h"
#include "i_soundfont.h"

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

static std::mutex patch_lock;
extern std::unique_ptr<FSoundFontReader> wm_sfreader;

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
	unsigned int sample_pos;
	unsigned int sample_inc;
	signed int env_inc;
	unsigned char env;
	signed int env_level;
	unsigned char modes;
	unsigned char hold;
	unsigned char active;
	struct _note *replay;
	struct _note *next;
	unsigned int left_mix_volume;
	unsigned int right_mix_volume;
	unsigned char is_off;
};

struct _event_data {
	unsigned char channel;
	unsigned int data;
};

struct _mdi {
	_mdi()
	{
		samples_to_mix = 0;
		midi_master_vol = 0;
		memset(&info, 0, sizeof(info));
		tmp_info = NULL;
		memset(&channel, 0, sizeof(channel));
		note = NULL;
		memset(note_table, 0, sizeof(note_table));
		patches = NULL;
		patch_count = 0;
		amp = 0;
		mix_buffer = NULL;
		mix_buffer_size = 0;
		reverb = NULL;
	}

	std::mutex lock;
	unsigned long int samples_to_mix;

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

#define FPBITS 10
#define FPMASK ((1L<<FPBITS)-1L)

/* Gauss Interpolation code adapted from code supplied by Eric. A. Welsh */
static double newt_coeffs[58][58];	/* for start/end of samples */
#define MAX_GAUSS_ORDER 34		/* 34 is as high as we can go before errors crop up */
static double *gauss_table = NULL;	/* *gauss_table[1<<FPBITS] */
static int gauss_n = MAX_GAUSS_ORDER;
static std::mutex gauss_lock;

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

	std::lock_guard<std::mutex> lock(gauss_lock);
	if (gauss_table) {
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
}

static void free_gauss(void) 
{
	std::lock_guard<std::mutex> lock(gauss_lock);
	free(gauss_table);
	gauss_table = NULL;
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

/* f: As per midi 2 standard */
static float dBm_volume[] = { -999999.999999f, -84.15214884f, -72.11094901f,
	-65.06729865f, -60.06974919f, -56.19334866f, -53.02609882f, -50.34822724f,
	-48.02854936f, -45.98244846f, -44.15214884f, -42.49644143f, -40.984899f,
	-39.59441475f, -38.30702741f, -37.10849848f, -35.98734953f, -34.93419198f,
	-33.94124863f, -33.0020048f, -32.11094901f, -31.26337705f, -30.45524161f,
	-29.6830354f, -28.94369917f, -28.23454849f, -27.55321492f, -26.89759827f,
	-26.26582758f, -25.65622892f, -25.06729865f, -24.49768108f, -23.94614971f,
	-23.41159124f, -22.89299216f, -22.38942706f, -21.90004881f, -21.42407988f,
	-20.96080497f, -20.50956456f, -20.06974919f, -19.64079457f, -19.22217722f,
	-18.81341062f, -18.41404178f, -18.02364829f, -17.64183557f, -17.26823452f,
	-16.90249934f, -16.54430564f, -16.19334866f, -15.84934179f, -15.51201509f,
	-15.18111405f, -14.85639845f, -14.53764126f, -14.22462776f, -13.91715461f,
	-13.6150291f, -13.31806837f, -13.02609882f, -12.73895544f, -12.45648126f,
	-12.17852686f, -11.90494988f, -11.63561457f, -11.37039142f, -11.10915673f,
	-10.85179233f, -10.59818521f, -10.34822724f, -10.10181489f, -9.858848981f,
	-9.619234433f, -9.382880049f, -9.149698303f, -8.919605147f, -8.692519831f,
	-8.468364731f, -8.247065187f, -8.028549359f, -7.812748083f, -7.599594743f,
	-7.389025143f, -7.180977396f, -6.97539181f, -6.772210788f, -6.571378733f,
	-6.372841952f, -6.176548572f, -5.982448461f, -5.790493145f, -5.600635744f,
	-5.412830896f, -5.227034694f, -5.043204627f, -4.861299517f, -4.681279468f,
	-4.503105811f, -4.326741054f, -4.152148838f, -3.979293887f, -3.808141968f,
	-3.63865985f, -3.470815266f, -3.304576875f, -3.139914228f, -2.976797731f,
	-2.815198619f, -2.655088921f, -2.496441432f, -2.339229687f, -2.183427931f,
	-2.029011099f, -1.875954785f, -1.724235224f, -1.573829269f, -1.424714368f,
	-1.276868546f, -1.130270383f, -0.9848989963f, -0.8407340256f, -0.6977556112f,
	-0.5559443807f, -0.4152814317f, -0.2757483179f, -0.1373270335f, 0 };

/* f: As per midi 2 standard */
static float dBm_pan_volume[127] = {
	-999999.999999f, -87.6945020928f, -73.8331126923f, -65.7264009888f,
	-59.9763864074f, -55.5181788833f, -51.8774481743f, -48.8011722841f,
	-46.1383198371f, -43.7914727130f, -41.6941147218f, -39.7988027954f,
	-38.0705069530f, -36.4826252703f, -35.0144798827f, -33.6496789707f,
	-32.3750084716f, -31.1796603753f, -30.0546819321f, -28.9925739783f,
	-27.9869924122f, -27.0325225804f, -26.1245061976f, -25.2589067713f,
	-24.4322036893f, -23.6413079424f, -22.8834943857f, -22.1563467917f,
	-21.4577129008f, -20.7856673630f, -20.1384809653f, -19.5145949062f,
	-18.9125991563f, -18.3312141503f, -17.7692752119f, -17.2257192381f,
	-16.6995732597f, -16.1899445690f, -15.6960121652f, -15.2170193110f,
	-14.7522670314f, -14.3011084168f, -13.8629436112f, -13.4372153915f,
	-13.0234052546f, -12.6210299451f, -12.2296383638f, -11.8488088095f,
	-11.4781465116f, -11.1172814164f, -10.7658661983f, -10.4235744668f,
	-10.0900991491f, -9.7651510261f, -9.4484574055f, -9.1397609172f,
	-8.8388184168f, -8.5453999868f, -8.2592880250f, -7.9802764101f,
	-7.7081697387f, -7.4427826248f, -7.1839390567f, -6.9314718056f,
	-6.6852218807f, -6.4450380272f, -6.2107762624f, -5.9822994468f,
	-5.7594768878f, -5.5421839719f, -5.3303018237f, -5.1237169899f,
	-4.9223211445f, -4.7260108155f, -4.5346871303f, -4.3482555779f,
	-4.1666257875f, -3.9897113219f, -3.8174294843f, -3.6497011373f,
	-3.4864505345f, -3.3276051620f, -3.1730955900f, -3.0228553340f,
	-2.8768207245f, -2.7349307844f, -2.5971271143f, -2.4633537845f,
	-2.3335572335f, -2.2076861725f, -2.0856914960f, -1.9675261968f,
	-1.8531452871f, -1.7425057233f, -1.6355663356f, -1.5322877618f,
	-1.4326323846f, -1.3365642732f, -1.2440491272f, -1.1550542250f,
	-1.0695483746f, -0.9875018671f, -0.9088864335f, -0.8336752037f,
	-0.7618426682f, -0.6933646420f, -0.6282182304f, -0.5663817981f,
	-0.5078349388f, -0.4525584478f, -0.4005342959f, -0.3517456058f,
	-0.3061766293f, -0.2638127266f, -0.2246403475f, -0.1886470134f,
	-0.1558213016f, -0.1261528303f, -0.0996322457f, -0.0762512098f,
	-0.0560023899f, -0.0388794497f, -0.0248770409f, -0.0139907967f,
	-0.0062173263f, -0.0015542108f, 0.0000000000f };

static unsigned int freq_table[] = { 837201792, 837685632, 838169728,
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

	std::lock_guard<std::mutex> lock(patch_lock);
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
static char** WM_LC_Tokenize_Line(char *line_data) 
{
	int line_length = (int)strlen(line_data);
	int token_data_length = 0;
	int line_ofs = 0;
	int token_start = 0;
	char **token_data = NULL;
	int token_count = 0;
	bool in_quotes = false;

	if (line_length == 0)
		return NULL;

	do {
		/* ignore everything after #  */
		if (line_data[line_ofs] == '#') {
			break;
		}
		if (line_data[line_ofs] == '"')
		{
			in_quotes = !in_quotes;
		}
		else if (!in_quotes && ((line_data[line_ofs] == ' ') || (line_data[line_ofs] == '\t'))) {
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

static int WM_LoadConfig(const char *config_file, bool main) {
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
	auto config_parm = config_file;

	FileReader fr;
	if (main)
	{
		if (!_WM_InitReader(config_file)) return -1;	// unable to open this as a config file.
		config_parm = nullptr;
	}

	config_buffer = (char *)_WM_BufferFile(config_parm, &config_size);
	if (!config_buffer) {
		WM_FreePatches();
		return -1;
	}

	FString bp = wm_sfreader->basePath();
	if (config_parm == nullptr) config_file = bp.GetChars();	// Re-get the base path because for archives this is empty.

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
						if (WM_LoadConfig(new_config, false) == -1) {
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

	std::lock_guard<std::mutex> lock(patch_lock);

	search_patch = patch[patchid & 0x007F];

	if (search_patch == NULL) {
		return NULL;
	}

	while (search_patch) {
		if (search_patch->patchid == patchid) {
			return search_patch;
		}
		search_patch = search_patch->next;
	}
	if ((patchid >> 8) != 0) {
		return (get_patch_data(patchid & 0x00FF));
	}
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

	std::lock_guard<std::mutex> lock(patch_lock);
	if (!tmp_patch->loaded) {
		if (load_sample(tmp_patch) == -1) {
			return;
		}
	}

	if (tmp_patch->first_sample == NULL) {
		return;
	}

	mdi->patch_count++;
	mdi->patches = (struct _patch**)realloc(mdi->patches,
			(sizeof(struct _patch*) * mdi->patch_count));
	mdi->patches[mdi->patch_count - 1] = tmp_patch;
	tmp_patch->inuse_count++;
}

static struct _sample *
get_sample_data(struct _patch *sample_patch, unsigned long int freq) {
	struct _sample *last_sample = NULL;
	struct _sample *return_sample = NULL;

	std::lock_guard<std::mutex> lock(patch_lock);
	if (sample_patch == NULL) {
		return NULL;
	}
	if (sample_patch->first_sample == NULL) {
		return NULL;
	}
	if (freq == 0) {
		return sample_patch->first_sample;
	}

	return_sample = sample_patch->first_sample;
	last_sample = sample_patch->first_sample;
	while (last_sample) {
		if (freq > last_sample->freq_low) {
			if (freq < last_sample->freq_high) {
				return last_sample;
			} else {
				return_sample = last_sample;
			}
		}
		last_sample = last_sample->next;
	}
	return return_sample;
}

/* Should be called in any function that effects note volumes */
void _WM_AdjustNoteVolumes(struct _mdi *mdi, unsigned char ch, struct _note *nte) {
    double premix_dBm;
    double premix_lin;
	int pan_ofs;
    double premix_dBm_left;
    double premix_dBm_right;
    double premix_left;
    double premix_right;
    double volume_adj;
    unsigned vol_ofs;

    /*
     Pointless CPU heating checks to shoosh up a compiler
     */
    if (ch > 0x0f) ch = 0x0f;

    pan_ofs = mdi->channel[ch].balance + mdi->channel[ch].pan - 64;

    vol_ofs = (nte->velocity * ((mdi->channel[ch].expression * mdi->channel[ch].volume) / 127)) / 127;

    /*
     This value is to reduce the chance of clipping.
     Higher value means lower overall volume,
     Lower value means higher overall volume.
     NOTE: The lower the value the higher the chance of clipping.
     FIXME: Still needs tuning. Clipping heard at a value of 3.75
     */
#define VOL_DIVISOR 4.0
    volume_adj = ((float)WM_MasterVolume / 1024.0) / VOL_DIVISOR;

	// Pan 0 and 1 are both hard left so 64 can be centered
    if (pan_ofs > 127) pan_ofs = 127;
	if (--pan_ofs < 0) pan_ofs = 0;
    premix_dBm_left = dBm_pan_volume[126-pan_ofs];
    premix_dBm_right = dBm_pan_volume[pan_ofs];

    if (mdi->info.mixer_options & WM_MO_LOG_VOLUME) {
        premix_dBm = dBm_volume[vol_ofs];

        premix_dBm_left += premix_dBm;
        premix_dBm_right += premix_dBm;

        premix_left = (pow(10.0,(premix_dBm_left / 20.0))) * volume_adj;
        premix_right = (pow(10.0,(premix_dBm_right / 20.0))) * volume_adj;
    } else {
        premix_lin = (float)(lin_volume[vol_ofs]) / 1024.0;

        premix_left = premix_lin * pow(10.0, (premix_dBm_left / 20)) * volume_adj;
        premix_right = premix_lin * pow(10.0, (premix_dBm_right / 20)) * volume_adj;
    }
    nte->left_mix_volume = (int)(premix_left * 1024.0);
    nte->right_mix_volume = (int)(premix_right * 1024.0);
}

/* Should be called in any function that effects channel volumes */
/* Calling this function with a value > 15 will make it adjust notes on all channels */
void _WM_AdjustChannelVolumes(struct _mdi *mdi, unsigned char ch) {
    struct _note *nte = mdi->note;
    if (nte != NULL) {
        do {
            if (ch <= 15) {
                if ((nte->noteid >> 8) == ch) {
                    goto _DO_ADJUST;
                }
            } else {
            _DO_ADJUST:
                _WM_AdjustNoteVolumes(mdi, ch, nte);
                if (nte->replay) _WM_AdjustNoteVolumes(mdi, ch, nte->replay);
            }
            nte = nte->next;
        } while (nte != NULL);
    }
}

static void do_note_off_extra(struct _note *nte) {

	nte->is_off = 0;


	if (!(nte->modes & SAMPLE_ENVELOPE)) {
		if (nte->modes & SAMPLE_LOOP) {
			nte->modes ^= SAMPLE_LOOP;
		}
		nte->env_inc = 0;

	} else if (nte->hold) {
		nte->hold |= HOLD_OFF;

	} else if (nte->modes & SAMPLE_SUSTAIN) {
		if (nte->env < 3) {
			nte->env = 3;
			if (nte->env_level > nte->sample->env_target[3]) {
				nte->env_inc = -nte->sample->env_rate[3];
			} else {
				nte->env_inc = nte->sample->env_rate[3];
			}
		}

	} else if (nte->modes & SAMPLE_CLAMPED) {
		if (nte->env < 5) {
			nte->env = 5;
			if (nte->env_level > nte->sample->env_target[5]) {
				nte->env_inc = -nte->sample->env_rate[5];
			} else {
				nte->env_inc = nte->sample->env_rate[5];
			}
		}
	} else if (nte->env < 4) {
		nte->env = 4;
		if (nte->env_level > nte->sample->env_target[4]) {
			nte->env_inc = -nte->sample->env_rate[4];
		} else {
			nte->env_inc = nte->sample->env_rate[4];
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

	if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env == 0)) {
		// This is a fix for notes that end before the
		// initial step of the envelope has completed
		// making it impossible to hear them at times.
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

static void do_note_on(struct _mdi *mdi, struct _event_data *data) {
	struct _note *nte;
	struct _note *prev_nte;
	struct _note *nte_array;
	unsigned long int freq = 0;
	struct _patch *patch;
	struct _sample *sample;
	unsigned char ch = data->channel;
	unsigned char note = (unsigned char)(data->data >> 8);
	unsigned char velocity = (unsigned char)(data->data & 0xFF);

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
	nte->replay = NULL;
	nte->is_off = 0;
	_WM_AdjustNoteVolumes(mdi, ch, nte);
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

	nte->velocity = (unsigned char)data->data;
	_WM_AdjustNoteVolumes(mdi, ch, nte);
	if (nte->replay) {
		nte->replay->velocity = (unsigned char)data->data;
		_WM_AdjustNoteVolumes(mdi, ch, nte->replay);
	}
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
	_WM_AdjustChannelVolumes(mdi, ch);
}

static void do_control_channel_balance(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;

	mdi->channel[ch].balance = (signed char)(data->data);
	_WM_AdjustChannelVolumes(mdi, ch);
}

static void do_control_channel_pan(struct _mdi *mdi, struct _event_data *data) {
	unsigned char ch = data->channel;

	mdi->channel[ch].pan = (signed char)(data->data);
	_WM_AdjustChannelVolumes(mdi, ch);
}

static void do_control_channel_expression(struct _mdi *mdi,
		struct _event_data *data) {
	struct _note *note_data = mdi->note;
	unsigned char ch = data->channel;

	mdi->channel[ch].expression = (unsigned char)data->data;
	_WM_AdjustChannelVolumes(mdi, ch);
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
static void do_control_non_registered_param_fine(struct _mdi *mdi,
		 struct _event_data *data) {
	unsigned char ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x3F80)
								| data->data;
	mdi->channel[ch].reg_non = 1;
}

static void do_control_non_registered_param_course(struct _mdi *mdi,
		struct _event_data *data) {
	unsigned char ch = data->channel;
	mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x7F)
								| (data->data << 7);
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
	mdi->channel[ch].reg_data = 0xffff;
	mdi->channel[ch].pitch_range = 200;
	mdi->channel[ch].pitch = 0;
	mdi->channel[ch].pitch_adjust = 0;
	mdi->channel[ch].hold = 0;

	_WM_AdjustChannelVolumes(mdi, ch);
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

	while (note_data) {
		if ((note_data->noteid >> 8) == ch) {
			note_data->velocity = (unsigned char)data->data;
			_WM_AdjustNoteVolumes(mdi, ch, note_data);
			if (note_data->replay) {
				note_data->replay->velocity = (unsigned char)data->data;
				_WM_AdjustNoteVolumes(mdi, ch, note_data->replay);
			}
		}
		note_data = note_data->next;
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

static void do_sysex_gm_reset(struct _mdi *mdi, struct _event_data *data) {
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
		mdi->channel[i].balance = 64;
		mdi->channel[i].pan = 64;
		mdi->channel[i].pitch = 0;
		mdi->channel[i].pitch_range = 200;
		mdi->channel[i].reg_data = 0xFFFF;
		mdi->channel[i].isdrum = 0;
	}
	/* I would not expect notes to be active when this event
	 triggers but we'll adjust active notes as well just in case */
	_WM_AdjustChannelVolumes(mdi,16); // A setting > 15 adjusts all channels

	mdi->channel[9].isdrum = 1;
	UNUSED(data); /* NOOP, to please the compiler gods */
}

static void do_sysex_roland_reset(struct _mdi *mdi, struct _event_data *data) {
	do_sysex_gm_reset(mdi, data);
}

static void do_sysex_yamaha_reset(struct _mdi *mdi, struct _event_data *data) {
	do_sysex_gm_reset(mdi, data);
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

	mdi = new _mdi;

	mdi->info.copyright = NULL;
	mdi->info.mixer_options = WM_MixerOptions;

	load_patch(mdi, 0x0000);

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
		std::lock_guard<std::mutex> lock(patch_lock);
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
		free(mdi->patches);
	}

	free(mdi->tmp_info);
	_WM_free_reverb(mdi->reverb);
	free(mdi->mix_buffer);
	delete mdi;
}

static int *WM_Mix_Linear(midi * handle, int * buffer, unsigned long int count)
{
	struct _mdi *mdi = (struct _mdi *)handle;
	unsigned long int data_pos;
	signed int premix, left_mix, right_mix;
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
				premix = ((note_data->sample->data[data_pos] + (((note_data->sample->data[data_pos + 1] - note_data->sample->data[data_pos]) * (int)(note_data->sample_pos & FPMASK)) / 1024)) * (note_data->env_level >> 12)) / 1024;


				left_mix += (premix * (int)note_data->left_mix_volume) / 1024;
				right_mix += (premix * (int)note_data->right_mix_volume) / 1024;

				/*
				 * ========================
				 * sample position checking
				 * ========================
				 */
				note_data->sample_pos += note_data->sample_inc;
				if (note_data->modes & SAMPLE_LOOP) {
					if (note_data->sample_pos > note_data->sample->loop_end) {
						note_data->sample_pos =
							note_data->sample->loop_start
									+ ((note_data->sample_pos
											- note_data->sample->loop_start)
											% note_data->sample->loop_size);
					}
				} else if (note_data->sample_pos >= note_data->sample->data_length) {
					goto END_THIS_NOTE;
				}

				if (note_data->env_inc == 0) {
					note_data = note_data->next;
					continue;
				}

				note_data->env_level += note_data->env_inc;
				if (note_data->env_inc < 0) {
					if (note_data->env_level > note_data->sample->env_target[note_data->env]) {
						note_data = note_data->next;
						continue;
					}
				} else if (note_data->env_inc > 0) {
					if (note_data->env_level < note_data->sample->env_target[note_data->env]) {
						note_data = note_data->next;
						continue;
					}
				}

				// Yes could have a condition here but
				// it would create another bottleneck
				note_data->env_level =
						note_data->sample->env_target[note_data->env];
				switch (note_data->env) {
				case 0:
					if (!(note_data->modes & SAMPLE_ENVELOPE)) {
						note_data->env_inc = 0;
						note_data = note_data->next;
						continue;
					}
					break;
				case 2:
					if (note_data->modes & SAMPLE_SUSTAIN /*|| note_data->hold*/) {
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
						goto END_THIS_NOTE;
					}
					/* sample release */
					if (note_data->modes & SAMPLE_LOOP)
						note_data->modes ^= SAMPLE_LOOP;
					note_data->env_inc = 0;
					note_data = note_data->next;
					continue;
				case 6:
					END_THIS_NOTE:
					if (note_data->replay != NULL) {
						note_data->active = 0;
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
						note_data->active = 0;
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

				premix = (int)((y * (note_data->env_level >> 12)) / 1024);

				left_mix += (premix * (int)note_data->left_mix_volume) / 1024;
				right_mix += (premix * (int)note_data->right_mix_volume) / 1024;

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
						goto END_THIS_NOTE;
					}
				}

				if (note_data->env_inc == 0) {
					note_data = note_data->next;
					continue;
				}

				note_data->env_level += note_data->env_inc;
				if (note_data->env_inc < 0) {
					if (note_data->env_level
						> note_data->sample->env_target[note_data->env]) {
						note_data = note_data->next;
						continue;
					}
				} else if (note_data->env_inc > 0) {
					if (note_data->env_level
						< note_data->sample->env_target[note_data->env]) {
						note_data = note_data->next;
						continue;
					}
				}

				// Yes could have a condition here but
				// it would create another bottleneck

				note_data->env_level =
						note_data->sample->env_target[note_data->env];
				switch (note_data->env) {
				case 0:
					if (!(note_data->modes & SAMPLE_ENVELOPE)) {
						note_data->env_inc = 0;
						note_data = note_data->next;
						continue;
					}
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
						goto END_THIS_NOTE;
					}
					/* sample release */
					if (note_data->modes & SAMPLE_LOOP)
						note_data->modes ^= SAMPLE_LOOP;
					note_data->env_inc = 0;
					note_data = note_data->next;
					continue;
				case 6:
					END_THIS_NOTE:
					if (note_data->replay != NULL) {
						note_data->active = 0;
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
						note_data->active = 0;
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
	if (WM_LoadConfig(config_file, true) == -1) {
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
	std::lock_guard<std::mutex> lock(mdi->lock);
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

	if ((((_mdi*)ret)->reverb = _WM_init_reverb(_WM_SampleRate, reverb_room_width,
			reverb_room_length, reverb_listen_posx, reverb_listen_posy))
			== NULL) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to init reverb", 0);
		WildMidi_Close(ret);
		ret = NULL;
	}


	return ret;
}

WM_SYMBOL int WildMidi_SetOption(midi * handle, unsigned short int options,
		unsigned short int setting) {
	struct _mdi *mdi;

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
	std::lock_guard<std::mutex> lock(mdi->lock);
	if ((!(options & 0x0007)) || (options & 0xFFF8)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG, "(invalid option)",
				0);
		return -1;
	}
	if (setting & 0xFFF8) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID_ARG,
				"(invalid setting)", 0);
		return -1;
	}

	mdi->info.mixer_options = ((mdi->info.mixer_options & (0x00FF ^ options))
			| (options & setting));

	if (options & WM_MO_LOG_VOLUME) {
		_WM_AdjustChannelVolumes(mdi, 16);	// Settings greater than 15
											// adjust all channels
	} else if (options & WM_MO_REVERB) {
		_WM_reset_reverb(mdi->reverb);
	}

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
	std::lock_guard<std::mutex> lock(mdi->lock);
	if (mdi->tmp_info == NULL) {
		mdi->tmp_info = (struct _WM_Info*)malloc(sizeof(struct _WM_Info));
		if (mdi->tmp_info == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to set info", 0);
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
		case 0:		do_control_bank_select(mdi, &ev);					break;
		case 6:		do_control_data_entry_course(mdi, &ev);				break;	// [sic]
		case 7:		do_control_channel_volume(mdi, &ev);				break;
		case 8:		do_control_channel_balance(mdi, &ev);				break;
		case 10:	do_control_channel_pan(mdi, &ev);					break;
		case 11:	do_control_channel_expression(mdi, &ev);			break;
		case 38:	do_control_data_entry_fine(mdi, &ev);				break;
		case 64:	do_control_channel_hold(mdi, &ev);					break;
		case 96:	do_control_data_increment(mdi, &ev);				break;
		case 97:	do_control_data_decrement(mdi, &ev);				break;
		case 98:	do_control_non_registered_param_fine(mdi, &ev);		break;
		case 99:	do_control_non_registered_param_course(mdi, &ev);	break;	// [sic]
		case 100:	do_control_registered_param_fine(mdi, &ev);			break;
		case 101:	do_control_registered_param_course(mdi, &ev);		break;	// [sic]
		case 120:	do_control_channel_sound_off(mdi, &ev);				break;
		case 121:	do_control_channel_controllers_off(mdi, &ev);		break;
		case 123:	do_control_channel_notes_off(mdi, &ev);				break;
		}
	}
}

void WildMidi_Renderer::LongEvent(const unsigned char *data, int len)
{
	// Check for Roland SysEx
	if (len >= 11 &&			// Must be at least 11 bytes
		data[len-1] == 0xF7 &&	// SysEx end
		data[0] == 0xF0 &&		// SysEx
		data[1] == 0x41 &&		// Roland
		data[2] == 0x10 &&		// Device ID, defaults to 0x10
		data[3] == 0x42 &&		// Model ID, 0x42 indicates a GS synth
		data[4] == 0x12 &&		// The other end is sending data to us
		data[5] == 0x40)		// We only care about addresses with this first byte
	{
		// Calculate checksum
		int cksum = 0;
		for (int i = 5; i < len - 2; ++i)
		{
			cksum += data[i];
		}
		cksum = 128 - (cksum & 0x7F);
		if (data[len-2] == cksum)
		{ // Check destination address
			if (((data[6] & 0xF0) == 0x10) && data[7] == 0x15)
			{ // Roland drum track setting
				unsigned char sysex_ch = data[6] & 0x0F;
				if (sysex_ch == 0)
				{
					sysex_ch = 9;
				}
				else if (sysex_ch <= 9)
				{
					sysex_ch -= 1;
				}
				_event_data ev = { sysex_ch, data[8] };
				do_sysex_roland_drum_track((_mdi *)handle, &ev);
			}
			else if (data[6] == 0x00 && data[7] == 0x7F && data[8] == 0x00)
			{ // Roland GS reset
				do_sysex_roland_reset((_mdi *)handle, NULL);
			}
		}
	}
	// For non-Roland Sysex messages */
	else
	{
		const unsigned char gm_reset[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7 };
		const unsigned char yamaha_reset[] = { 0xf0, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7};
		if (len == 6 && memcmp(gm_reset, data, 6) == 0)
		{
			do_sysex_gm_reset((_mdi *)handle, NULL);
		}
		else if (len == 9 && memcmp(yamaha_reset, data, 9) == 0)
		{
			do_sysex_yamaha_reset((_mdi *)handle, NULL);
		}
	}
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
		*(float *)buffer = (float)*buffer * (1.3f / 32768.f);	// boost the volume because Wildmidi is far more quiet than the other synths and therefore hard to balance.
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

void WildMidi_Renderer::SetOption(int opt, int set)
{
	WildMidi_SetOption((_mdi*)handle, (unsigned short)opt, (unsigned short)set);
}
