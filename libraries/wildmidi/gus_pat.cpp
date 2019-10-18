/*
 * gus_pat.c -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2001-2015
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gus_pat.h"
#include "common.h"
#include "wm_error.h"
#include "file_io.h"
#include "wildmidi_lib.h"

namespace WildMidi
{

	/* Guspat Envelope Rate Timings */
	
	float env_time_table[] = {
		/* Row 1 = (4095.0 / (x * ( 1.0 / (1.6 * 14.0)         ))) / 1000000.0 */
		0.0f,         0.091728000f, 0.045864000f, 0.030576000f, 0.022932000f, 0.018345600f, 0.015288000f, 0.013104000f,
		0.011466000f, 0.010192000f, 0.009172800f, 0.008338909f, 0.007644000f, 0.007056000f, 0.006552000f, 0.006115200f,
		0.005733000f, 0.005395765f, 0.005096000f, 0.004827789f, 0.004586400f, 0.004368000f, 0.004169455f, 0.003988174f,
		0.003822000f, 0.003669120f, 0.003528000f, 0.003397333f, 0.003276000f, 0.003163034f, 0.003057600f, 0.002958968f,
		0.002866500f, 0.002779636f, 0.002697882f, 0.002620800f, 0.002548000f, 0.002479135f, 0.002413895f, 0.002352000f,
		0.002293200f, 0.002237268f, 0.002184000f, 0.002133209f, 0.002084727f, 0.002038400f, 0.001994087f, 0.001951660f,
		0.001911000f, 0.001872000f, 0.001834560f, 0.001798588f, 0.001764000f, 0.001730717f, 0.001698667f, 0.001667782f,
		0.001638000f, 0.001609263f, 0.001581517f, 0.001554712f, 0.001528800f, 0.001503738f, 0.001479484f, 0.001456000f,
		
		/* Row 2 = (4095.0 / (x * ((1.0 / (1.6 * 14.0)) / 8.0  ))) / 1000000.0 */
		0.0f,         0.733824000f, 0.366912000f, 0.244608000f, 0.183456000f, 0.146764800f, 0.122304000f, 0.104832000f,
		0.091728000f, 0.081536000f, 0.073382400f, 0.066711273f, 0.061152000f, 0.056448000f, 0.052416000f, 0.048921600f,
		0.045864000f, 0.043166118f, 0.040768000f, 0.038622316f, 0.036691200f, 0.034944000f, 0.033355636f, 0.031905391f,
		0.030576000f, 0.029352960f, 0.028224000f, 0.027178667f, 0.026208000f, 0.025304276f, 0.024460800f, 0.023671742f,
		0.022932000f, 0.022237091f, 0.021583059f, 0.020966400f, 0.020384000f, 0.019833081f, 0.019311158f, 0.018816000f,
		0.018345600f, 0.017898146f, 0.017472000f, 0.017065674f, 0.016677818f, 0.016307200f, 0.015952696f, 0.015613277f,
		0.015288000f, 0.014976000f, 0.014676480f, 0.014388706f, 0.014112000f, 0.013845736f, 0.013589333f, 0.013342255f,
		0.013104000f, 0.012874105f, 0.012652138f, 0.012437695f, 0.012230400f, 0.012029902f, 0.011835871f, 0.011648000f,
		
		/* Row 3 = (4095.0 / (x * ((1.0 / (1.6 * 14.0)) / 64.0 ))) / 1000000.0 */
		0.0f,         5.870592000f, 2.935296000f, 1.956864000f, 1.467648000f, 1.174118400f, 0.978432000f, 0.838656000f,
		0.733824000f, 0.652288000f, 0.587059200f, 0.533690182f, 0.489216000f, 0.451584000f, 0.419328000f, 0.391372800f,
		0.366912000f, 0.345328941f, 0.326144000f, 0.308978526f, 0.293529600f, 0.279552000f, 0.266845091f, 0.255243130f,
		0.244608000f, 0.234823680f, 0.225792000f, 0.217429333f, 0.209664000f, 0.202434207f, 0.195686400f, 0.189373935f,
		0.183456000f, 0.177896727f, 0.172664471f, 0.167731200f, 0.163072000f, 0.158664649f, 0.154489263f, 0.150528000f,
		0.146764800f, 0.143185171f, 0.139776000f, 0.136525395f, 0.133422545f, 0.130457600f, 0.127621565f, 0.124906213f,
		0.122304000f, 0.119808000f, 0.117411840f, 0.115109647f, 0.112896000f, 0.110765887f, 0.108714667f, 0.106738036f,
		0.104832000f, 0.102992842f, 0.101217103f, 0.099501559f, 0.097843200f, 0.096239213f, 0.094686968f, 0.093184000f,
		
		/* Row 4 = (4095.0 / (x * ((1.0 / (1.6 * 14.0)) / 512.0))) / 1000000.0 */
		0.0f,        46.964736000f,23.482368000f,15.654912000f,11.741184000f, 9.392947200f, 7.827456000f, 6.709248000f,
		5.870592000f, 5.218304000f, 4.696473600f, 4.269521455f, 3.913728000f, 3.612672000f, 3.354624000f, 3.130982400f,
		2.935296000f, 2.762631529f, 2.609152000f, 2.471828211f, 2.348236800f, 2.236416000f, 2.134760727f, 2.041945043f,
		1.956864000f, 1.878589440f, 1.806336000f, 1.739434667f, 1.677312000f, 1.619473655f, 1.565491200f, 1.514991484f,
		1.467648000f, 1.423173818f, 1.381315765f, 1.341849600f, 1.304576000f, 1.269317189f, 1.235914105f, 1.204224000f,
		1.174118400f, 1.145481366f, 1.118208000f, 1.092203163f, 1.067380364f, 1.043660800f, 1.020972522f, 0.999249702f,
		0.978432000f, 0.958464000f, 0.939294720f, 0.920877176f, 0.903168000f, 0.886127094f, 0.869717333f, 0.853904291f,
		0.838656000f, 0.823942737f, 0.809736828f, 0.796012475f, 0.782745600f, 0.769913705f, 0.757495742f, 0.745472000f
	};
#ifdef DEBUG_GUSPAT
#define GUSPAT_FILENAME_DEBUG(dx) fprintf(stderr,"\r%s\n",dx)

#define GUSPAT_INT_DEBUG(dx,dy) fprintf(stderr,"\r%s: %i\n",dx,dy)
#define GUSPAT_FLOAT_DEBUG(dx,dy) fprintf(stderr,"\r%s: %f\n",dx,dy)
#define GUSPAT_START_DEBUG() fprintf(stderr,"\r")
#define GUSPAT_MODE_DEBUG(dx,dy,dz) if (dx & dy) fprintf(stderr,"%s",dz)
#define GUSPAT_END_DEBUG() fprintf(stderr,"\n")
#else
#define GUSPAT_FILENAME_DEBUG(dx)
#define GUSPAT_INT_DEBUG(dx,dy)
#define GUSPAT_FLOAT_DEBUG(dx,dy)
#define GUSPAT_START_DEBUG()
#define GUSPAT_MODE_DEBUG(dx,dy,dz)
#define GUSPAT_END_DEBUG()
#endif

#ifdef DEBUG_SAMPLES
#define SAMPLE_CONVERT_DEBUG(dx) printf("\r%s\n",dx)
#else
#define SAMPLE_CONVERT_DEBUG(dx)
#endif
/*
 * sample data conversion functions
 * convert data to signed shorts
 */

/* 8bit signed */
static int convert_8s(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((gus_sample->data_length + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data++ = (*read_data++) << 8;
		} while (read_data != read_end);
		return 0;
	}

	_WM_ERROR_NEW("Calloc failed (%s)\n", strerror(errno));
	return -1;
}

/* 8bit signed ping pong */
static int convert_8sp(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((new_length + 2), sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data++ = (*read_data++) << 8;
		} while (read_data != read_end);

		*write_data = (*read_data++ << 8);
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = (*read_data++) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data != read_end);

		*write_data = (*read_data++ << 8);
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (read_data != read_end) {
			do {
				*write_data_b++ = (*read_data++) << 8;
			} while (read_data != read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit signed reverse */
static int convert_8sr(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((gus_sample->data_length + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data + gus_sample->data_length - 1;
		do {
			*write_data-- = (*read_data++) << 8;
		} while (read_data != read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
				| ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->modes ^= SAMPLE_REVERSE;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit signed reverse ping pong */
static int convert_8srp(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((new_length + 2), sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data++ = (*read_data--) << 8;
		} while (read_data != read_end);

		*write_data = (*read_data-- << 8);
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = (*read_data--) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data != read_end);

		*write_data = (*read_data-- << 8);
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b++ = (*read_data--) << 8;
			write_data_b++;
		} while (read_data != read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned */
static int convert_8u(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((gus_sample->data_length + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data++ = ((*read_data++) ^ 0x80) << 8;
		} while (read_data != read_end);
		gus_sample->modes ^= SAMPLE_UNSIGNED;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned ping pong */
static int convert_8up(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((new_length + 2), sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data++ = ((*read_data++) ^ 0x80) << 8;
		} while (read_data != read_end);

		*write_data = ((*read_data++) ^ 0x80) << 8;
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = ((*read_data++) ^ 0x80) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data != read_end);

		*write_data = ((*read_data++) ^ 0x80) << 8;
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (read_data != read_end) {
			do {
				*write_data_b++ = ((*read_data++) ^ 0x80) << 8;
			} while (read_data != read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_UNSIGNED;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned reverse */
static int convert_8ur(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((gus_sample->data_length + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data + gus_sample->data_length - 1;
		do {
			*write_data-- = ((*read_data++) ^ 0x80) << 8;
		} while (read_data != read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
				| ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->modes ^= SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 8bit unsigned reverse ping pong */
static int convert_8urp(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc((new_length + 2), sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data++ = ((*read_data--) ^ 0x80) << 8;
		} while (read_data != read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		write_data_a = write_data + dloop_length;
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + dloop_length;
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data != read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b++ = ((*read_data--) ^ 0x80) << 8;
		} while (read_data != read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed */
static int convert_16s(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((gus_sample->data_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data = *read_data++;
			*write_data++ |= (*read_data++) << 8;
		} while (read_data < read_end);

		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed ping pong */
static int convert_16sp(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((new_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data++);
			*write_data++ |= (*read_data++) << 8;
		} while (read_data < read_end);

		*write_data = (*read_data++);
		*write_data |= (*read_data++) << 8;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = (*read_data++);
			*write_data |= (*read_data++) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data < read_end);

		*write_data = *(read_data++);
		*write_data |= (*read_data++) << 8;
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (read_data != read_end) {
			do {
				*write_data_b = *(read_data++);
				*write_data_b++ |= (*read_data++) << 8;
			} while (read_data < read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG;
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed reverse */
static int convert_16sr(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((gus_sample->data_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data + (gus_sample->data_length >> 1) - 1;
		do {
			*write_data = *read_data++;
			*write_data-- |= (*read_data++) << 8;
		} while (read_data < read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
				| ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		gus_sample->modes ^= SAMPLE_REVERSE;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit signed reverse ping pong */
static int convert_16srp(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((new_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data--) << 8;
			*write_data++ |= *read_data--;
		} while (read_data < read_end);

		*write_data = (*read_data-- << 8);
		*write_data |= *read_data--;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = (*read_data--) << 8;
			*write_data |= *read_data--;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data < read_end);

		*write_data = ((*read_data--) << 8);
		*write_data |= *read_data--;
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b = (*read_data--) << 8;
			*write_data_b++ |= *read_data--;
		} while (read_data < read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned */
static int convert_16u(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((gus_sample->data_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data = *read_data++;
			*write_data++ |= ((*read_data++) ^ 0x80) << 8;
		} while (read_data < read_end);
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		gus_sample->modes ^= SAMPLE_UNSIGNED;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned ping pong */
static int convert_16up(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->loop_start;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((new_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data = (*read_data++);
			*write_data++ |= ((*read_data++) ^ 0x80) << 8;
		} while (read_data < read_end);

		*write_data = (*read_data++);
		*write_data |= ((*read_data++) ^ 0x80) << 8;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_end;
		do {
			*write_data = (*read_data++);
			*write_data |= ((*read_data++) ^ 0x80) << 8;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data < read_end);

		*write_data = (*read_data++);
		*write_data |= ((*read_data++) ^ 0x80) << 8;
		*write_data_b++ = *write_data;
		read_end = data + gus_sample->data_length;
		if (read_data != read_end) {
			do {
				*write_data_b = (*read_data++);
				*write_data_b++ |= ((*read_data++) ^ 0x80) << 8;
			} while (read_data < read_end);
		}
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG;
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned reverse */
static int convert_16ur(unsigned char *data, struct _sample *gus_sample) {
	unsigned char *read_data = data;
	unsigned char *read_end = data + gus_sample->data_length;
	signed short int *write_data = NULL;
	unsigned long int tmp_loop = 0;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((gus_sample->data_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data + (gus_sample->data_length >> 1) - 1;
		do {
			*write_data = *read_data++;
			*write_data-- |= ((*read_data++) ^ 0x80) << 8;
		} while (read_data < read_end);
		tmp_loop = gus_sample->loop_end;
		gus_sample->loop_end = gus_sample->data_length - gus_sample->loop_start;
		gus_sample->loop_start = gus_sample->data_length - tmp_loop;
		gus_sample->loop_fraction = ((gus_sample->loop_fraction & 0x0f) << 4)
				| ((gus_sample->loop_fraction & 0xf0) >> 4);
		gus_sample->loop_start >>= 1;
		gus_sample->loop_end >>= 1;
		gus_sample->data_length >>= 1;
		gus_sample->modes ^= SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}
	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* 16bit unsigned reverse ping pong */
static int convert_16urp(unsigned char *data, struct _sample *gus_sample) {
	unsigned long int loop_length = gus_sample->loop_end
			- gus_sample->loop_start;
	unsigned long int dloop_length = loop_length * 2;
	unsigned long int new_length = gus_sample->data_length + dloop_length;
	unsigned char *read_data = data + gus_sample->data_length - 1;
	unsigned char *read_end = data + gus_sample->loop_end;
	signed short int *write_data = NULL;
	signed short int *write_data_a = NULL;
	signed short int *write_data_b = NULL;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__);
	gus_sample->data = (short*)calloc(((new_length >> 1) + 2),
			sizeof(signed short int));
	if (gus_sample->data != NULL) {
		write_data = gus_sample->data;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			*write_data++ |= *read_data--;
		} while (read_data < read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		*write_data |= *read_data--;
		write_data_a = write_data + (dloop_length >> 1);
		*write_data_a-- = *write_data;
		write_data++;
		write_data_b = write_data + (dloop_length >> 1);
		read_end = data + gus_sample->loop_start;
		do {
			*write_data = ((*read_data--) ^ 0x80) << 8;
			*write_data |= *read_data--;
			*write_data_a-- = *write_data;
			*write_data_b++ = *write_data;
			write_data++;
		} while (read_data < read_end);

		*write_data = ((*read_data--) ^ 0x80) << 8;
		*write_data |= *read_data--;
		*write_data_b++ = *write_data;
		read_end = data - 1;
		do {
			*write_data_b = ((*read_data--) ^ 0x80) << 8;
			*write_data_b++ |= *read_data--;
		} while (read_data < read_end);
		gus_sample->loop_start += loop_length;
		gus_sample->loop_end += dloop_length;
		gus_sample->data_length = new_length;
		gus_sample->modes ^= SAMPLE_PINGPONG | SAMPLE_REVERSE | SAMPLE_UNSIGNED;
		return 0;
	}

	_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, "to parse sample", errno);
	return -1;
}

/* sample loading */

struct _sample * Instruments::load_gus_pat(const char *filename)
{
	unsigned char *gus_patch;
	unsigned long int gus_size;
	unsigned long int gus_ptr;
	unsigned char no_of_samples;
	struct _sample *gus_sample = NULL;
	struct _sample *first_gus_sample = NULL;
	unsigned long int i = 0;

	int (*do_convert[])(unsigned char *data, struct _sample *gus_sample) = {
		convert_8s,
		convert_16s,
		convert_8u,
		convert_16u,
		convert_8sp,
		convert_16sp,
		convert_8up,
		convert_16up,
		convert_8sr,
		convert_16sr,
		convert_8ur,
		convert_16ur,
		convert_8srp,
		convert_16srp,
		convert_8urp,
		convert_16urp
	};
	unsigned long int tmp_loop;

	SAMPLE_CONVERT_DEBUG(__FUNCTION__); SAMPLE_CONVERT_DEBUG(filename);

	if ((gus_patch = _WM_BufferFile(sfreader, filename, &gus_size)) == NULL) {
		return NULL;
	}
	if (gus_size < 239) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(too short)", 0);
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, 0);
		free(gus_patch);
		return NULL;
	}
	if (memcmp(gus_patch, "GF1PATCH110\0ID#000002", 22)
			&& memcmp(gus_patch, "GF1PATCH100\0ID#000002", 22)) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(unsupported format)",
				0);
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, 0);
		free(gus_patch);
		return NULL;
	}
	if (gus_patch[82] > 1) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(unsupported format)",
				0);
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, 0);
		free(gus_patch);
		return NULL;
	}
	if (gus_patch[151] > 1) {
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_INVALID, "(unsupported format)",
				0);
		_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, 0);
		free(gus_patch);
		return NULL;
	}

	GUSPAT_FILENAME_DEBUG(filename); GUSPAT_INT_DEBUG("voices",gus_patch[83]);

	no_of_samples = gus_patch[198];
	gus_ptr = 239;
	while (no_of_samples) {
		unsigned long int tmp_cnt;
		if (first_gus_sample == NULL) {
			first_gus_sample = (struct _sample*)malloc(sizeof(struct _sample));
			gus_sample = first_gus_sample;
		} else {
			gus_sample->next = (struct _sample*)malloc(sizeof(struct _sample));
			gus_sample = gus_sample->next;
		}
		if (gus_sample == NULL) {
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_MEM, NULL, 0);
			_WM_ERROR(__FUNCTION__, __LINE__, WM_ERR_LOAD, filename, 0);
			free(gus_patch);
			return NULL;
		}

		gus_sample->next = NULL;
		gus_sample->loop_fraction = gus_patch[gus_ptr + 7];
		gus_sample->data_length = (gus_patch[gus_ptr + 11] << 24)
				| (gus_patch[gus_ptr + 10] << 16)
				| (gus_patch[gus_ptr + 9] << 8) | gus_patch[gus_ptr + 8];
		gus_sample->loop_start = (gus_patch[gus_ptr + 15] << 24)
				| (gus_patch[gus_ptr + 14] << 16)
				| (gus_patch[gus_ptr + 13] << 8) | gus_patch[gus_ptr + 12];
		gus_sample->loop_end = (gus_patch[gus_ptr + 19] << 24)
				| (gus_patch[gus_ptr + 18] << 16)
				| (gus_patch[gus_ptr + 17] << 8) | gus_patch[gus_ptr + 16];
		gus_sample->rate = (gus_patch[gus_ptr + 21] << 8)
				| gus_patch[gus_ptr + 20];
		gus_sample->freq_low = ((gus_patch[gus_ptr + 25] << 24)
				| (gus_patch[gus_ptr + 24] << 16)
				| (gus_patch[gus_ptr + 23] << 8) | gus_patch[gus_ptr + 22]);
		gus_sample->freq_high = ((gus_patch[gus_ptr + 29] << 24)
				| (gus_patch[gus_ptr + 28] << 16)
				| (gus_patch[gus_ptr + 27] << 8) | gus_patch[gus_ptr + 26]);
		gus_sample->freq_root = ((gus_patch[gus_ptr + 33] << 24)
				| (gus_patch[gus_ptr + 32] << 16)
				| (gus_patch[gus_ptr + 31] << 8) | gus_patch[gus_ptr + 30]);

		/* This is done this way instead of ((freq * 1024) / rate) to avoid 32bit overflow. */
		/* Result is 0.001% inacurate */
		gus_sample->inc_div = ((gus_sample->freq_root * 512) / gus_sample->rate) * 2;

#if 0
		/* We dont use this info at this time, kept in here for info */
		printf("\rTremolo Sweep: %i, Rate: %i, Depth %i\n",
				gus_patch[gus_ptr+49], gus_patch[gus_ptr+50], gus_patch[gus_ptr+51]);
		printf("\rVibrato Sweep: %i, Rate: %i, Depth %i\n",
				gus_patch[gus_ptr+52], gus_patch[gus_ptr+53], gus_patch[gus_ptr+54]);
#endif
		gus_sample->modes = gus_patch[gus_ptr + 55];
		GUSPAT_START_DEBUG(); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_16BIT, "16bit "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_UNSIGNED, "Unsigned "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_LOOP, "Loop "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_PINGPONG, "PingPong "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_REVERSE, "Reverse "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_SUSTAIN, "Sustain "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_ENVELOPE, "Envelope "); GUSPAT_MODE_DEBUG(gus_patch[gus_ptr+55], SAMPLE_CLAMPED, "Clamped "); GUSPAT_END_DEBUG();

		if (gus_sample->loop_start > gus_sample->loop_end) {
			tmp_loop = gus_sample->loop_end;
			gus_sample->loop_end = gus_sample->loop_start;
			gus_sample->loop_start = tmp_loop;
			gus_sample->loop_fraction =
					((gus_sample->loop_fraction & 0x0f) << 4)
							| ((gus_sample->loop_fraction & 0xf0) >> 4);
		}

		/*
		 FIXME: Experimental Hacky Fix

		 This looks for "dodgy" release envelope settings that faulty editors
		 may have set and attempts to corrects it.
		 if (fix_release)
		 Lets make this automatic ...
		 */
		{
			/*
			 After studying faulty gus_pats this way may work better
			 Testing to determine if any further adjustments are required
			*/
			if (env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 41]]) {
				unsigned char tmp_hack_rate = 0;

				if (env_time_table[gus_patch[gus_ptr + 41]] < env_time_table[gus_patch[gus_ptr + 42]]) {
					// 1 2 3
					tmp_hack_rate = gus_patch[gus_ptr + 40];
					gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 42];
					gus_patch[gus_ptr + 42] = tmp_hack_rate;
				} else if (env_time_table[gus_patch[gus_ptr + 41]] == env_time_table[gus_patch[gus_ptr + 42]]) {
					// 1 2 2
					tmp_hack_rate = gus_patch[gus_ptr + 40];
					gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 42];
					gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 42];
					gus_patch[gus_ptr + 42] = tmp_hack_rate;

				} else {
					if (env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 42]]) {
						// 1 3 2
						tmp_hack_rate = gus_patch[gus_ptr + 40];
						gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 41];
						gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 42];
						gus_patch[gus_ptr + 42] = tmp_hack_rate;
					} else {
						// 2 3 1 or 1 2 1
						tmp_hack_rate = gus_patch[gus_ptr + 40];
						gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 41];
						gus_patch[gus_ptr + 41] = tmp_hack_rate;
					}
				}
			} else if (env_time_table[gus_patch[gus_ptr + 41]] < env_time_table[gus_patch[gus_ptr + 42]]) {
				unsigned char tmp_hack_rate = 0;

				if (env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 42]]) {
					// 2 1 3
					tmp_hack_rate = gus_patch[gus_ptr + 40];
					gus_patch[gus_ptr + 40] = gus_patch[gus_ptr + 42];
					gus_patch[gus_ptr + 42] = gus_patch[gus_ptr + 41];
					gus_patch[gus_ptr + 41] = tmp_hack_rate;
				} else {
					// 3 1 2
					tmp_hack_rate = gus_patch[gus_ptr + 41];
					gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 42];
					gus_patch[gus_ptr + 42] = tmp_hack_rate;
				}
			}

#if 0
			if ((env_time_table[gus_patch[gus_ptr + 40]] < env_time_table[gus_patch[gus_ptr + 41]]) && (env_time_table[gus_patch[gus_ptr + 41]] == env_time_table[gus_patch[gus_ptr + 42]])) {
				uint8_t tmp_hack_rate = 0;
				tmp_hack_rate = gus_patch[gus_ptr + 41];
				gus_patch[gus_ptr + 41] = gus_patch[gus_ptr + 40];
				gus_patch[gus_ptr + 42] = gus_patch[gus_ptr + 40];
				gus_patch[gus_ptr + 40] = tmp_hack_rate;
				tmp_hack_rate = gus_patch[gus_ptr + 47];
				gus_patch[gus_ptr + 47] = gus_patch[gus_ptr + 46];
				gus_patch[gus_ptr + 48] = gus_patch[gus_ptr + 46];
				gus_patch[gus_ptr + 46] = tmp_hack_rate;
			}
#endif
		}

		for (i = 0; i < 6; i++) {
			GUSPAT_INT_DEBUG("Envelope #",i);
			if (gus_sample->modes & SAMPLE_ENVELOPE) {
				unsigned char env_rate = gus_patch[gus_ptr + 37 + i];
				gus_sample->env_target[i] = 16448 * gus_patch[gus_ptr + 43 + i];
				GUSPAT_INT_DEBUG("Envelope Level",gus_patch[gus_ptr+43+i]); GUSPAT_FLOAT_DEBUG("Envelope Time",env_time_table[env_rate]);
				gus_sample->env_rate[i] = (signed long int) (4194303.0
						/ ((float) _WM_SampleRate * env_time_table[env_rate]));
				GUSPAT_INT_DEBUG("Envelope Rate",gus_sample->env_rate[i]); GUSPAT_INT_DEBUG("GUSPAT Rate",env_rate);
				if (gus_sample->env_rate[i] == 0) {
					_WM_ERROR_NEW("Warning: found invalid envelope(%lu) rate setting in %s. Using %f instead.\n",
							i, filename, env_time_table[63]);
					gus_sample->env_rate[i] = (signed long int) (4194303.0
							/ ((float) _WM_SampleRate * env_time_table[63]));
					GUSPAT_FLOAT_DEBUG("Envelope Time",env_time_table[63]);
				}
			} else {
				gus_sample->env_target[i] = 4194303;
				gus_sample->env_rate[i] = (signed long int) (4194303.0
						/ ((float) _WM_SampleRate * env_time_table[63]));
				GUSPAT_FLOAT_DEBUG("Envelope Time",env_time_table[63]);
			}
		}

		gus_sample->env_target[6] = 0;
		gus_sample->env_rate[6] = (signed long int) (4194303.0
				/ ((float) _WM_SampleRate * env_time_table[63]));

		gus_ptr += 96;
		tmp_cnt = gus_sample->data_length;

		if (do_convert[(((gus_sample->modes & 0x18) >> 1)
				| (gus_sample->modes & 0x03))](&gus_patch[gus_ptr], gus_sample)
				== -1) {
			free(gus_patch);
			return NULL;
		}

		/*
		 Test and set decay expected decay time after a note off
		 NOTE: This sets samples for full range decay
		 */
		if (gus_sample->modes & SAMPLE_ENVELOPE) {
			double samples_f = 0.0;

			if (gus_sample->modes & SAMPLE_CLAMPED) {
				samples_f = (4194301.0 - (float)gus_sample->env_target[5]) / gus_sample->env_rate[5];
			} else {
				if (gus_sample->modes & SAMPLE_SUSTAIN) {
					samples_f = (4194301.0 - (float)gus_sample->env_target[3]) / gus_sample->env_rate[3];
					samples_f += (float)(gus_sample->env_target[3] - gus_sample->env_target[4]) / gus_sample->env_rate[4];
				} else {
					samples_f = (4194301.0 - (float)gus_sample->env_target[4]) / gus_sample->env_rate[4];
				}
				samples_f += (float)(gus_sample->env_target[4] - gus_sample->env_target[5]) / gus_sample->env_rate[5];
			}
			samples_f += (float)gus_sample->env_target[5] / gus_sample->env_rate[6];
		}

		gus_ptr += tmp_cnt;
		gus_sample->loop_start = (gus_sample->loop_start << 10)
				| (((gus_sample->loop_fraction & 0x0f) << 10) / 16);
		gus_sample->loop_end = (gus_sample->loop_end << 10)
				| (((gus_sample->loop_fraction & 0xf0) << 6) / 16);
		gus_sample->loop_size = gus_sample->loop_end - gus_sample->loop_start;
		gus_sample->data_length = gus_sample->data_length << 10;
		no_of_samples--;
	}
	free(gus_patch);
	return first_gus_sample;
}

}
