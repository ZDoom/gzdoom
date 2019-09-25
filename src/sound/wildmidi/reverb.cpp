/*
 reverb.c

 Midi Wavetable Processing library

 Copyright (C) Chris Ison  2001-2011
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

#include <math.h>
#include <stdlib.h>

#include "common.h"
#include "reverb.h"

namespace WildMidi
{
/*
 reverb function
 */
void _WM_reset_reverb(struct _rvb *rvb) {
	int i, j, k;
	for (i = 0; i < rvb->l_buf_size; i++) {
		rvb->l_buf[i] = 0;
	}
	for (i = 0; i < rvb->r_buf_size; i++) {
		rvb->r_buf[i] = 0;
	}
	for (k = 0; k < 8; k++) {
		for (i = 0; i < 6; i++) {
			for (j = 0; j < 2; j++) {
				rvb->l_buf_flt_in[k][i][j] = 0;
				rvb->l_buf_flt_out[k][i][j] = 0;
				rvb->r_buf_flt_in[k][i][j] = 0;
				rvb->r_buf_flt_out[k][i][j] = 0;
			}
		}
	}
}

/*
 _WM_init_reverb

 =========================
 Engine Description

 8 reflective points around the room
 2 speaker positions
 1 listener position

 Sounds come from the speakers to all points and to the listener.
 Sound comes from the reflective points to the listener.
 These sounds are combined, put through a filter that mimics surface absorbtion.
 The combined sounds are also sent to the reflective points on the opposite side.

 */
struct _rvb *
_WM_init_reverb(int rate, float room_x, float room_y, float listen_x,
		float listen_y) {

	/* filters set at 125Hz, 250Hz, 500Hz, 1000Hz, 2000Hz, 4000Hz */
	double Freq[] = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0};

	/* numbers calculated from
	 * 101.325 kPa, 20 deg C, 50% relative humidity */
	double dbAirAbs[] = {-0.00044, -0.00131, -0.002728, -0.004665, -0.009887, -0.029665};

	/* modify these to adjust the absorption qualities of the surface.
	 * Remember that lower frequencies are less effected by surfaces
	 * Note: I am currently playing with the values and finding the ideal surfaces
	 * for nice default reverb.
	 */
	double dbAttn[8][6] = {
		{-1.839, -6.205, -8.891, -12.059, -15.935, -20.942},
		{-0.131, -6.205, -12.059, -20.933, -20.933, -15.944},
		{-0.131, -6.205, -12.059, -20.933, -20.933, -15.944},
		{-1.839, -6.205, -8.891, -12.059, -15.935, -20.942},
		{-1.839, -6.205, -8.891, -12.059, -15.935, -20.942},
		{-0.131, -6.205, -12.059, -20.933, -20.933, -15.944},
		{-0.131, -6.205, -12.059, -20.933, -20.933, -15.944},
		{-1.839, -6.205, -8.891, -12.059, -15.935, -20.942}
	};
	/*
	double dbAttn[6] = {
	// concrete covered in carpet
	//	-0.175, -0.537, -1.412, -4.437, -7.959, -7.959
	// pleated drapes
		-0.630, -3.223, -5.849, -12.041, -10.458, -7.959
	};
	*/

	/* distance */
	double SPL_DST[8] = {0.0};
	double SPR_DST[8] = {0.0};
	double RFN_DST[8] = {0.0};

	double MAXL_DST = 0.0;
	double MAXR_DST = 0.0;

	double SPL_LSN_XOFS = 0.0;
	double SPL_LSN_YOFS = 0.0;
	double SPL_LSN_DST = 0.0;

	double SPR_LSN_XOFS = 0.0;
	double SPR_LSN_YOFS = 0.0;
	double SPR_LSN_DST = 0.0;


	struct _rvb *rtn_rvb = (struct _rvb*)malloc(sizeof(struct _rvb));
	int j = 0;
	int i = 0;

	struct _coord {
		double x;
		double y;
	};

#if 0
	struct _coord SPL = {2.5, 5.0}; /* Left Speaker Position */
	struct _coord SPR = {7.5, 5.0}; /* Right Speaker Position */
	/* position of the reflective points */
	struct _coord RFN[] = {
		{	5.0, 0.0},
		{	0.0, 6.66666},
		{	0.0, 13.3333},
		{	5.0, 20.0},
		{	10.0, 20.0},
		{	15.0, 13.3333},
		{	15.0, 6.66666},
		{	10.0, 0.0}
	};
#else
	struct _coord SPL; /* Left Speaker Position */
	struct _coord SPR; /* Right Speaker Position */
	/* position of the reflective points */
	struct _coord RFN[8];

	SPL.x = room_x / 4.0;
	SPR.x = room_x / 4.0 * 3.0;
	SPL.y = room_y / 10.0;
	SPR.y = room_y / 10.0;

	RFN[0].x = room_x / 3.0;
	RFN[0].y = 0.0;
	RFN[1].x = 0.0;
	RFN[1].y = room_y / 3.0;
	RFN[2].x = 0.0;
	RFN[2].y = room_y / 3.0 * 2.0;
	RFN[3].x = room_x / 3.0;
	RFN[3].y = room_y;
	RFN[4].x = room_x / 3.0 * 2.0;
	RFN[4].y = room_y;
	RFN[5].x = room_x;
	RFN[5].y = room_y / 3.0 * 2.0;
	RFN[6].x = room_x;
	RFN[6].y = room_y / 3.0;
	RFN[7].x = room_x / 3.0 * 2.0;
	RFN[7].y = 0.0;
#endif

	SPL_LSN_XOFS = SPL.x - listen_x;
	SPL_LSN_YOFS = SPL.y - listen_y;
	SPL_LSN_DST = sqrt((SPL_LSN_XOFS * SPL_LSN_XOFS) + (SPL_LSN_YOFS * SPL_LSN_YOFS));

	if (SPL_LSN_DST > MAXL_DST)
		MAXL_DST = SPL_LSN_DST;

	SPR_LSN_XOFS = SPR.x - listen_x;
	SPR_LSN_YOFS = SPR.y - listen_y;
	SPR_LSN_DST = sqrt((SPR_LSN_XOFS * SPR_LSN_XOFS) + (SPR_LSN_YOFS * SPR_LSN_YOFS));

	if (SPR_LSN_DST > MAXR_DST)
		MAXR_DST = SPR_LSN_DST;

	if (rtn_rvb == NULL) {
		return NULL;
	}

	for (j = 0; j < 8; j++) {
		double SPL_RFL_XOFS = 0;
		double SPL_RFL_YOFS = 0;
		double SPR_RFL_XOFS = 0;
		double SPR_RFL_YOFS = 0;
		double RFN_XOFS = listen_x - RFN[j].x;
		double RFN_YOFS = listen_y - RFN[j].y;
		RFN_DST[j] = sqrt((RFN_XOFS * RFN_XOFS) + (RFN_YOFS * RFN_YOFS));

		SPL_RFL_XOFS = SPL.x - RFN[i].x;
		SPL_RFL_YOFS = SPL.y - RFN[i].y;
		SPR_RFL_XOFS = SPR.x - RFN[i].x;
		SPR_RFL_YOFS = SPR.y - RFN[i].y;
		SPL_DST[i] = sqrt(
				(SPL_RFL_XOFS * SPL_RFL_XOFS) + (SPL_RFL_YOFS * SPL_RFL_YOFS));
		SPR_DST[i] = sqrt(
				(SPR_RFL_XOFS * SPR_RFL_XOFS) + (SPR_RFL_YOFS * SPR_RFL_YOFS));
		/*
		 add the 2 distances together and remove the speaker to listener distance
		 so we dont have to delay the initial output
		 */
		SPL_DST[i] += RFN_DST[i];

		/* so i dont have to delay speaker output */
		SPL_DST[i] -= SPL_LSN_DST;

		if (i < 4) {
			if (SPL_DST[i] > MAXL_DST)
				MAXL_DST = SPL_DST[i];
		} else {
			if (SPL_DST[i] > MAXR_DST)
				MAXR_DST = SPL_DST[i];
		}

		SPR_DST[i] += RFN_DST[i];

		/* so i dont have to delay speaker output */
		SPR_DST[i] -= SPR_LSN_DST;

		if (i < 4) {
			if (SPR_DST[i] > MAXL_DST)
				MAXL_DST = SPR_DST[i];
		} else {
			if (SPR_DST[i] > MAXR_DST)
				MAXR_DST = SPR_DST[i];
		}

		RFN_DST[j] *= 2.0;

		if (j < 4) {
			if (RFN_DST[j] > MAXL_DST)
				MAXL_DST = RFN_DST[j];
		} else {
			if (RFN_DST[j] > MAXR_DST)
				MAXR_DST = RFN_DST[j];
		}

		for (i = 0; i < 6; i++) {
			double srate = (double) rate;
			double bandwidth = 2.0;
			double omega = 2.0 * M_PI * Freq[i] / srate;
			double sn = sin(omega);
			double cs = cos(omega);
			double alpha = sn * sinh(M_LN2 / 2 * bandwidth * omega / sn);
			double A = pow(10.0, ((/*dbAttn[i]*/dbAttn[j][i] +
						(dbAirAbs[i] * RFN_DST[j])) / 40.0) );
			/*
			 Peaking band EQ filter
			 */
			double b0 = 1 + (alpha * A);
			double b1 = -2 * cs;
			double b2 = 1 - (alpha * A);
			double a0 = 1 + (alpha / A);
			double a1 = -2 * cs;
			double a2 = 1 - (alpha / A);

			rtn_rvb->coeff[j][i][0] = (signed int) ((b0 / a0) * 1024.0);
			rtn_rvb->coeff[j][i][1] = (signed int) ((b1 / a0) * 1024.0);
			rtn_rvb->coeff[j][i][2] = (signed int) ((b2 / a0) * 1024.0);
			rtn_rvb->coeff[j][i][3] = (signed int) ((a1 / a0) * 1024.0);
			rtn_rvb->coeff[j][i][4] = (signed int) ((a2 / a0) * 1024.0);
		}
	}

	/* init the reverb buffers */
	rtn_rvb->l_buf_size = (int) ((float) rate * (MAXL_DST / 340.29));
	rtn_rvb->l_buf = (int*)malloc(
			sizeof(signed int) * (rtn_rvb->l_buf_size + 1));
	rtn_rvb->l_out = 0;

	rtn_rvb->r_buf_size = (int) ((float) rate * (MAXR_DST / 340.29));
	rtn_rvb->r_buf = (int*)malloc(
			sizeof(signed int) * (rtn_rvb->r_buf_size + 1));
	rtn_rvb->r_out = 0;

	for (i = 0; i < 4; i++) {
		rtn_rvb->l_sp_in[i] = (int) ((float) rate * (SPL_DST[i] / 340.29));
		rtn_rvb->l_sp_in[i + 4] = (int) ((float) rate
				* (SPL_DST[i + 4] / 340.29));
		rtn_rvb->r_sp_in[i] = (int) ((float) rate * (SPR_DST[i] / 340.29));
		rtn_rvb->r_sp_in[i + 4] = (int) ((float) rate
				* (SPR_DST[i + 4] / 340.29));
		rtn_rvb->l_in[i] = (int) ((float) rate * (RFN_DST[i] / 340.29));
		rtn_rvb->r_in[i] = (int) ((float) rate * (RFN_DST[i + 4] / 340.29));
	}

	rtn_rvb->gain = 4;

	_WM_reset_reverb(rtn_rvb);
	return rtn_rvb;
}

/* _WM_free_reverb - free up memory used for reverb */
void _WM_free_reverb(struct _rvb *rvb) {
	if (!rvb) return;
	free(rvb->l_buf);
	free(rvb->r_buf);
	free(rvb);
}

void _WM_do_reverb(struct _rvb *rvb, signed int *buffer, int size) {
	int i, j, k;
	signed int l_buf_flt = 0;
	signed int r_buf_flt = 0;
	signed int l_rfl = 0;
	signed int r_rfl = 0;
	int vol_div = 64;

	for (i = 0; i < size; i += 2) {
		signed int tmp_l_val = 0;
		signed int tmp_r_val = 0;
		/*
		 add the initial reflections
		 from each speaker, 4 to go the left, 4 go to the right buffers
		 */
		tmp_l_val = buffer[i] / vol_div;
		tmp_r_val = buffer[i + 1] / vol_div;
		for (j = 0; j < 4; j++) {
			rvb->l_buf[rvb->l_sp_in[j]] += tmp_l_val;
			rvb->l_sp_in[j] = (rvb->l_sp_in[j] + 1) % rvb->l_buf_size;
			rvb->l_buf[rvb->r_sp_in[j]] += tmp_r_val;
			rvb->r_sp_in[j] = (rvb->r_sp_in[j] + 1) % rvb->l_buf_size;

			rvb->r_buf[rvb->l_sp_in[j + 4]] += tmp_l_val;
			rvb->l_sp_in[j + 4] = (rvb->l_sp_in[j + 4] + 1) % rvb->r_buf_size;
			rvb->r_buf[rvb->r_sp_in[j + 4]] += tmp_r_val;
			rvb->r_sp_in[j + 4] = (rvb->r_sp_in[j + 4] + 1) % rvb->r_buf_size;
		}

		/*
		 filter the reverb output and add to buffer
		 */
		l_rfl = rvb->l_buf[rvb->l_out];
		rvb->l_buf[rvb->l_out] = 0;
		rvb->l_out = (rvb->l_out + 1) % rvb->l_buf_size;

		r_rfl = rvb->r_buf[rvb->r_out];
		rvb->r_buf[rvb->r_out] = 0;
		rvb->r_out = (rvb->r_out + 1) % rvb->r_buf_size;

		for (k = 0; k < 8; k++) {
			for (j = 0; j < 6; j++) {
				l_buf_flt = ((l_rfl * rvb->coeff[k][j][0])
						+ (rvb->l_buf_flt_in[k][j][0] * rvb->coeff[k][j][1])
						+ (rvb->l_buf_flt_in[k][j][1] * rvb->coeff[k][j][2])
						- (rvb->l_buf_flt_out[k][j][0] * rvb->coeff[k][j][3])
						- (rvb->l_buf_flt_out[k][j][1] * rvb->coeff[k][j][4]))
						/ 1024;
				rvb->l_buf_flt_in[k][j][1] = rvb->l_buf_flt_in[k][j][0];
				rvb->l_buf_flt_in[k][j][0] = l_rfl;
				rvb->l_buf_flt_out[k][j][1] = rvb->l_buf_flt_out[k][j][0];
				rvb->l_buf_flt_out[k][j][0] = l_buf_flt;
				buffer[i] += l_buf_flt / 8;

				r_buf_flt = ((r_rfl * rvb->coeff[k][j][0])
						+ (rvb->r_buf_flt_in[k][j][0] * rvb->coeff[k][j][1])
						+ (rvb->r_buf_flt_in[k][j][1] * rvb->coeff[k][j][2])
						- (rvb->r_buf_flt_out[k][j][0] * rvb->coeff[k][j][3])
						- (rvb->r_buf_flt_out[k][j][1] * rvb->coeff[k][j][4]))
						/ 1024;
				rvb->r_buf_flt_in[k][j][1] = rvb->r_buf_flt_in[k][j][0];
				rvb->r_buf_flt_in[k][j][0] = r_rfl;
				rvb->r_buf_flt_out[k][j][1] = rvb->r_buf_flt_out[k][j][0];
				rvb->r_buf_flt_out[k][j][0] = r_buf_flt;
				buffer[i + 1] += r_buf_flt / 8;
			}
		}

		/*
		 add filtered result back into the buffers but on the opposite side
		 */
		tmp_l_val = buffer[i + 1] / vol_div;
		tmp_r_val = buffer[i] / vol_div;
		for (j = 0; j < 4; j++) {
			rvb->l_buf[rvb->l_in[j]] += tmp_l_val;
			rvb->l_in[j] = (rvb->l_in[j] + 1) % rvb->l_buf_size;

			rvb->r_buf[rvb->r_in[j]] += tmp_r_val;
			rvb->r_in[j] = (rvb->r_in[j] + 1) % rvb->r_buf_size;
		}
	}
}

}
