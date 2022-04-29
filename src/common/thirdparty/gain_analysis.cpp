/*
 *  ReplayGainAnalysis - analyzes input samples and give the recommended dB change
 *  Copyright (C) 2001-2009 David Robinson and Glen Sawyer
 *  Improvements and optimizations added by Frank Klemm, and by Marcel M�ller 
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  concept and filter values by David Robinson (David@Robinson.org)
 *    -- blame him if you think the idea is flawed
 *  original coding by Glen Sawyer (mp3gain@hotmail.com)
 *    -- blame him if you think this runs too slowly, or the coding is otherwise flawed
 *
 *  lots of code improvements by Frank Klemm ( http://www.uni-jena.de/~pfk/mpp/ )
 *    -- credit him for all the _good_ programming ;)
 *
 *
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

/*
 *  Here's the deal. Call
 *
 *    InitGainAnalysis ( long samplefreq );
 *
 *  to initialize everything. Call
 *
 *    AnalyzeSamples ( const Float_t*  left_samples,
 *                     const Float_t*  right_samples,
 *                     size_t          num_samples,
 *                     int             num_channels );
 *
 *  as many times as you want, with as many or as few samples as you want.
 *  If mono, pass the sample buffer in through left_samples, leave
 *  right_samples NULL, and make sure num_channels = 1.
 *
 *    GetTitleGain()
 *
 *  will return the recommended dB level change for all samples analyzed
 *  SINCE THE LAST TIME you called GetTitleGain() OR InitGainAnalysis().
 *
 *    GetAlbumGain()
 *
 *  will return the recommended dB level change for all samples analyzed
 *  since InitGainAnalysis() was called and finalized with GetTitleGain().
 *
 *  Pseudo-code to process an album:
 *
 *    Float_t       l_samples [4096];
 *    Float_t       r_samples [4096];
 *    size_t        num_samples;
 *    unsigned int  num_songs;
 *    unsigned int  i;
 *
 *    InitGainAnalysis ( 44100 );
 *    for ( i = 1; i <= num_songs; i++ ) {
 *        while ( ( num_samples = getSongSamples ( song[i], left_samples, right_samples ) ) > 0 )
 *            AnalyzeSamples ( left_samples, right_samples, num_samples, 2 );
 *        fprintf ("Recommended dB change for song %2d: %+6.2f dB\n", i, GetTitleGain() );
 *    }
 *    fprintf ("Recommended dB change for whole album: %+6.2f dB\n", GetAlbumGain() );
 */

/*
 *  So here's the main source of potential code confusion:
 *
 *  The filters applied to the incoming samples are IIR filters,
 *  meaning they rely on up to <filter order> number of previous samples
 *  AND up to <filter order> number of previous filtered samples.
 *
 *  I set up the AnalyzeSamples routine to minimize memory usage and interface
 *  complexity. The speed isn't compromised too much (I don't think), but the
 *  internal complexity is higher than it should be for such a relatively
 *  simple routine.
 *
 *  Optimization/clarity suggestions are welcome.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "gain_analysis.h"

#define RMS_PERCENTILE      0.95        // percentile which is louder than the proposed level

#define PINK_REF                64.82 //298640883795                              // calibration value


// for each filter:
// [0] 48 kHz, [1] 44.1 kHz, [2] 32 kHz, [3] 24 kHz, [4] 22050 Hz, [5] 16 kHz, [6] 12 kHz, [7] is 11025 Hz, [8] 8 kHz

#ifdef WIN32
#ifndef __GNUC__
#pragma warning ( disable : 4305 )
#pragma warning ( disable : 4244 )
#endif
#endif

static const Float_t ABYule[][2 * YULE_ORDER + 1] = {
        {(Float_t) 0.006471345933032, (Float_t) -7.22103125152679, (Float_t) -0.02567678242161,  (Float_t) 24.7034187975904,  (Float_t) 0.049805860704367, (Float_t) -52.6825833623896,  (Float_t) -0.05823001743528,  (Float_t) 77.4825736677539,  (Float_t) 0.040611847441914, (Float_t) -82.0074753444205,  (Float_t) -0.010912036887501, (Float_t) 63.1566097101925,  (Float_t) -0.00901635868667, (Float_t) -34.889569769245,  (Float_t) 0.012448886238123,  (Float_t) 13.2126852760198,  (Float_t) -0.007206683749426, (Float_t) -3.09445623301669, (Float_t) 0.002167156433951,  (Float_t) 0.340344741393305, (Float_t) -0.000261819276949},
        {(Float_t) 0.015415414474287, (Float_t) -7.19001570087017, (Float_t) -0.07691359399407,  (Float_t) 24.4109412087159,  (Float_t) 0.196677418516518, (Float_t) -51.6306373580801,  (Float_t) -0.338855114128061, (Float_t) 75.3978476863163,  (Float_t) 0.430094579594561, (Float_t) -79.4164552507386,  (Float_t) -0.415015413747894, (Float_t) 61.0373661948115,  (Float_t) 0.304942508151101, (Float_t) -33.7446462547014, (Float_t) -0.166191795926663, (Float_t) 12.8168791146274,  (Float_t) 0.063198189938739,  (Float_t) -3.01332198541437, (Float_t) -0.015003978694525, (Float_t) 0.223619893831468, (Float_t) 0.001748085184539},
        {(Float_t) 0.021776466467053, (Float_t) -5.74819833657784, (Float_t) -0.062376961003801, (Float_t) 16.246507961894,   (Float_t) 0.107731165328514, (Float_t) -29.9691822642542,  (Float_t) -0.150994515142316, (Float_t) 40.027597579378,   (Float_t) 0.170334807313632, (Float_t) -40.3209196052655,  (Float_t) -0.157984942890531, (Float_t) 30.8542077487718,  (Float_t) 0.121639833268721, (Float_t) -17.5965138737281, (Float_t) -0.074094040816409, (Float_t) 7.10690214103873,  (Float_t) 0.031282852041061,  (Float_t) -1.82175564515191, (Float_t) -0.00755421235941,  (Float_t) 0.223619893831468, (Float_t) 0.00117925454213},
        {(Float_t) 0.03857599435200,  (Float_t) -3.84664617118067, (Float_t) -0.02160367184185,  (Float_t) 7.81501653005538,  (Float_t) -0.00123395316851, (Float_t) -11.34170355132042, (Float_t) -0.00009291677959,  (Float_t) 13.05504219327545, (Float_t) -0.01655260341619, (Float_t) -12.28759895145294, (Float_t) 0.02161526843274,   (Float_t) 9.48293806319790,  (Float_t) -0.02074045215285, (Float_t) -5.87257861775999, (Float_t) 0.00594298065125,   (Float_t) 2.75465861874613,  (Float_t) 0.00306428023191,   (Float_t) -0.86984376593551, (Float_t) 0.00012025322027,   (Float_t) 0.13919314567432,  (Float_t) 0.00288463683916},
        {(Float_t) 0.05418656406430,  (Float_t) -3.47845948550071, (Float_t) -0.02911007808948,  (Float_t) 6.36317777566148,  (Float_t) -0.00848709379851, (Float_t) -8.54751527471874,  (Float_t) -0.00851165645469,  (Float_t) 9.47693607801280,  (Float_t) -0.00834990904936, (Float_t) -8.81498681370155,  (Float_t) 0.02245293253339,   (Float_t) 6.85401540936998,  (Float_t) -0.02596338512915, (Float_t) -4.39470996079559, (Float_t) 0.01624864962975,   (Float_t) 2.19611684890774,  (Float_t) -0.00240879051584,  (Float_t) -0.75104302451432, (Float_t) 0.00674613682247,   (Float_t) 0.13149317958808,  (Float_t) -0.00187763777362},
        {(Float_t) 0.15457299681924,  (Float_t) -2.37898834973084, (Float_t) -0.09331049056315,  (Float_t) 2.84868151156327,  (Float_t) -0.06247880153653, (Float_t) -2.64577170229825,  (Float_t) 0.02163541888798,   (Float_t) 2.23697657451713,  (Float_t) -0.05588393329856, (Float_t) -1.67148153367602,  (Float_t) 0.04781476674921,   (Float_t) 1.00595954808547,  (Float_t) 0.00222312597743,  (Float_t) -0.45953458054983, (Float_t) 0.03174092540049,   (Float_t) 0.16378164858596,  (Float_t) -0.01390589421898,  (Float_t) -0.05032077717131, (Float_t) 0.00651420667831,   (Float_t) 0.02347897407020,  (Float_t) -0.00881362733839},
        {(Float_t) 0.30296907319327,  (Float_t) -1.61273165137247, (Float_t) -0.22613988682123,  (Float_t) 1.07977492259970,  (Float_t) -0.08587323730772, (Float_t) -0.25656257754070,  (Float_t) 0.03282930172664,   (Float_t) -0.16276719120440, (Float_t) -0.00915702933434, (Float_t) -0.22638893773906,  (Float_t) -0.02364141202522,  (Float_t) 0.39120800788284,  (Float_t) -0.00584456039913, (Float_t) -0.22138138954925, (Float_t) 0.06276101321749,   (Float_t) 0.04500235387352,  (Float_t) -0.00000828086748,  (Float_t) 0.02005851806501,  (Float_t) 0.00205861885564,   (Float_t) 0.00302439095741,  (Float_t) -0.02950134983287},
        {(Float_t) 0.33642304856132,  (Float_t) -1.49858979367799, (Float_t) -0.25572241425570,  (Float_t) 0.87350271418188,  (Float_t) -0.11828570177555, (Float_t) 0.12205022308084,   (Float_t) 0.11921148675203,   (Float_t) -0.80774944671438, (Float_t) -0.07834489609479, (Float_t) 0.47854794562326,   (Float_t) -0.00469977914380,  (Float_t) -0.12453458140019, (Float_t) -0.00589500224440, (Float_t) -0.04067510197014, (Float_t) 0.05724228140351,   (Float_t) 0.08333755284107,  (Float_t) 0.00832043980773,   (Float_t) -0.04237348025746, (Float_t) -0.01635381384540,  (Float_t) 0.02977207319925,  (Float_t) -0.01760176568150},
        {(Float_t) 0.44915256608450,  (Float_t) -0.62820619233671, (Float_t) -0.14351757464547,  (Float_t) 0.29661783706366,  (Float_t) -0.22784394429749, (Float_t) -0.37256372942400,  (Float_t) -0.01419140100551,  (Float_t) 0.00213767857124,  (Float_t) 0.04078262797139,  (Float_t) -0.42029820170918,  (Float_t) -0.12398163381748,  (Float_t) 0.22199650564824,  (Float_t) 0.04097565135648,  (Float_t) 0.00613424350682,  (Float_t) 0.10478503600251,   (Float_t) 0.06747620744683,  (Float_t) -0.01863887810927,  (Float_t) 0.05784820375801,  (Float_t) -0.03193428438915,  (Float_t) 0.03222754072173,  (Float_t) 0.00541907748707},
        {(Float_t) 0.56619470757641,  (Float_t) -1.04800335126349, (Float_t) -0.75464456939302,  (Float_t) 0.29156311971249,  (Float_t) 0.16242137742230,  (Float_t) -0.26806001042947,  (Float_t) 0.16744243493672,   (Float_t) 0.00819999645858,  (Float_t) -0.18901604199609, (Float_t) 0.45054734505008,   (Float_t) 0.30931782841830,   (Float_t) -0.33032403314006, (Float_t) -0.27562961986224, (Float_t) 0.06739368333110,  (Float_t) 0.00647310677246,   (Float_t) -0.04784254229033, (Float_t) 0.08647503780351,   (Float_t) 0.01639907836189,  (Float_t) -0.03788984554840,  (Float_t) 0.01807364323573,  (Float_t) -0.00588215443421},
        {(Float_t) 0.58100494960553,  (Float_t) -0.51035327095184, (Float_t) -0.53174909058578,  (Float_t) -0.31863563325245, (Float_t) -0.14289799034253, (Float_t) -0.20256413484477,  (Float_t) 0.17520704835522,   (Float_t) 0.14728154134330,  (Float_t) 0.02377945217615,  (Float_t) 0.38952639978999,   (Float_t) 0.15558449135573,   (Float_t) -0.23313271880868, (Float_t) -0.25344790059353, (Float_t) -0.05246019024463, (Float_t) 0.01628462406333,   (Float_t) -0.02505961724053, (Float_t) 0.06920467763959,   (Float_t) 0.02442357316099,  (Float_t) -0.03721611395801,  (Float_t) 0.01818801111503,  (Float_t) -0.00749618797172},
        {(Float_t) 0.53648789255105,  (Float_t) -0.25049871956020, (Float_t) -0.42163034350696,  (Float_t) -0.43193942311114, (Float_t) -0.00275953611929, (Float_t) -0.03424681017675,  (Float_t) 0.04267842219415,   (Float_t) -0.04678328784242, (Float_t) -0.10214864179676, (Float_t) 0.26408300200955,   (Float_t) 0.14590772289388,   (Float_t) 0.15113130533216,  (Float_t) -0.02459864859345, (Float_t) -0.17556493366449, (Float_t) -0.11202315195388,  (Float_t) -0.18823009262115, (Float_t) -0.04060034127000,  (Float_t) 0.05477720428674,  (Float_t) 0.04788665548180,   (Float_t) 0.04704409688120,  (Float_t) -0.02217936801134},

        {(Float_t) 0.38524531015142,  (Float_t) -1.29708918404534, (Float_t) -0.27682212062067,  (Float_t) 0.90399339674203,  (Float_t)-0.09980181488805,  (Float_t) -0.29613799017877,  (Float_t) 0.09951486755646,   (Float_t)-0.42326645916207,  (Float_t) -0.08934020156622, (Float_t) 0.37934887402200,   (Float_t) -0.00322369330199,  (Float_t) -0.37919795944938, (Float_t) -0.00110329090689, (Float_t) 0.23410283284785, (Float_t) 0.03784509844682, (Float_t) -0.03892971758879, (Float_t) 0.01683906213303, (Float_t) 0.00403009552351, (Float_t) -0.01147039862572, (Float_t) 0.03640166626278, (Float_t) -0.01941767987192 },
        {(Float_t)0.08717879977844, (Float_t)-2.62816311472146, (Float_t)-0.01000374016172, (Float_t)3.53734535817992, (Float_t)-0.06265852122368, (Float_t)-3.81003448678921, (Float_t)-0.01119328800950, (Float_t)3.91291636730132, (Float_t)-0.00114279372960, (Float_t)-3.53518605896288, (Float_t)0.02081333954769, (Float_t)2.71356866157873, (Float_t)-0.01603261863207, (Float_t)-1.86723311846592, (Float_t)0.01936763028546, (Float_t)1.12075382367659, (Float_t)0.00760044736442, (Float_t)-0.48574086886890, (Float_t)-0.00303979112271, (Float_t)0.11330544663849, (Float_t)-0.00075088605788 },

};


static const Float_t ABButter[][2 * BUTTER_ORDER + 1] = {
        {(Float_t) 0.99308203517541,  (Float_t) -1.98611621154089, (Float_t) -1.98616407035082, (Float_t) 0.986211929160751, (Float_t) 0.99308203517541},
        {(Float_t) 0.992472550461293, (Float_t) -1.98488843762334, (Float_t) -1.98494510092258, (Float_t) 0.979389350028798, (Float_t) 0.992472550461293},
        {(Float_t) 0.989641019334721, (Float_t) -1.97917472731008, (Float_t) -1.97928203866944, (Float_t) 0.979389350028798, (Float_t) 0.989641019334721},
        {(Float_t) 0.98621192462708,  (Float_t) -1.97223372919527, (Float_t) -1.97242384925416, (Float_t) 0.97261396931306,  (Float_t) 0.98621192462708},
        {(Float_t) 0.98500175787242,  (Float_t) -1.96977855582618, (Float_t) -1.97000351574484, (Float_t) 0.97022847566350,  (Float_t) 0.98500175787242},
        {(Float_t) 0.97938932735214,  (Float_t) -1.95835380975398, (Float_t) -1.95877865470428, (Float_t) 0.95920349965459,  (Float_t) 0.97938932735214},
        {(Float_t) 0.97531843204928,  (Float_t) -1.95002759149878, (Float_t) -1.95063686409857, (Float_t) 0.95124613669835,  (Float_t) 0.97531843204928},
        {(Float_t) 0.97316523498161,  (Float_t) -1.94561023566527, (Float_t) -1.94633046996323, (Float_t) 0.94705070426118,  (Float_t) 0.97316523498161},
        {(Float_t) 0.96454515552826,  (Float_t) -1.92783286977036, (Float_t) -1.92909031105652, (Float_t) 0.93034775234268,  (Float_t) 0.96454515552826},
        {(Float_t) 0.96009142950541,  (Float_t) -1.91858953033784, (Float_t) -1.92018285901082, (Float_t) 0.92177618768381,  (Float_t) 0.96009142950541},
        {(Float_t) 0.95856916599601,  (Float_t) -1.91542108074780, (Float_t) -1.91713833199203, (Float_t) 0.91885558323625,  (Float_t) 0.95856916599601},
        {(Float_t) 0.94597685600279,  (Float_t) -1.88903307939452, (Float_t) -1.89195371200558, (Float_t) 0.89487434461664,  (Float_t) 0.94597685600279},

        {(Float_t)0.96535326815829, (Float_t)-1.92950577983524, (Float_t)-1.93070653631658, (Float_t)0.93190729279793, (Float_t)0.96535326815829 },
        {(Float_t)0.98252400815195, (Float_t)-1.96474258269041, (Float_t)-1.96504801630391, (Float_t)0.96535344991740, (Float_t)0.98252400815195 },

};

#ifdef WIN32
#ifndef __GNUC__
#pragma warning ( default : 4305 )
#endif
#endif

// When calling these filter procedures, make sure that ip[-order] and op[-order] point to real data!

// If your compiler complains that "'operation on 'output' may be undefined", you can
// either ignore the warnings or uncomment the three "y" lines (and comment out the indicated line)

void
GainAnalyzer::filterYule(const Float_t *input, Float_t *output, size_t nSamples, const Float_t *kernel) 
{

    while (nSamples--) {
        *output = 1e-10f  /* 1e-10 is a hack to avoid slowdown because of denormals */
                  + input[0] * kernel[0]
                  - output[-1] * kernel[1]
                  + input[-1] * kernel[2]
                  - output[-2] * kernel[3]
                  + input[-2] * kernel[4]
                  - output[-3] * kernel[5]
                  + input[-3] * kernel[6]
                  - output[-4] * kernel[7]
                  + input[-4] * kernel[8]
                  - output[-5] * kernel[9]
                  + input[-5] * kernel[10]
                  - output[-6] * kernel[11]
                  + input[-6] * kernel[12]
                  - output[-7] * kernel[13]
                  + input[-7] * kernel[14]
                  - output[-8] * kernel[15]
                  + input[-8] * kernel[16]
                  - output[-9] * kernel[17]
                  + input[-9] * kernel[18]
                  - output[-10] * kernel[19]
                  + input[-10] * kernel[20];
        ++output;
        ++input;
    }
}

void
GainAnalyzer::filterButter(const Float_t *input, Float_t *output, size_t nSamples, const Float_t *kernel) {

    while (nSamples--) {
        *output =
                input[0] * kernel[0]
                - output[-1] * kernel[1]
                + input[-1] * kernel[2]
                - output[-2] * kernel[3]
                + input[-2] * kernel[4];
        ++output;
        ++input;
    }
}


// returns a INIT_GAIN_ANALYSIS_OK if successful, INIT_GAIN_ANALYSIS_ERROR if not

int
GainAnalyzer::ResetSampleFrequency(int samplefreq) {
    int i;

    // zero out initial values
    for (i = 0; i < MAX_ORDER; i++)
        linprebuf[i] = lstepbuf[i] = loutbuf[i] = rinprebuf[i] = rstepbuf[i] = routbuf[i] = (Float_t) 0.;

    switch ((int) (samplefreq)) {
        case 96000:
            freqindex = 0;
            break;
        case 88200:
            freqindex = 1;
            break;
        case 64000:
            freqindex = 2;
            break;
        case 49716: // I could not find a table for this but we need to be able to handle this frequency for OPL, even if this means not getting the proper level.
        case 48000:
            freqindex = 3;
            break;
        case 44100:
            freqindex = 4;
            break;
        case 32000:
            freqindex = 5;
            break;
        case 24000:
            freqindex = 6;
            break;
        case 22050:
            freqindex = 7;
            break;
        case 16000:
            freqindex = 8;
            break;
        case 12000:
            freqindex = 9;
            break;
        case 11025:
        case 11111: // SW shareware tries to play a VOC with this frequency as music. This is close enough to 11025 to use the same table.
            freqindex = 10;
            break;
        case 8000:
            freqindex = 11;
            break;

        // These two were added for XA support.
        case 18900:
            freqindex = 12;
            break;
        case 37800:
            freqindex = 13;
            break;
        default:
            return INIT_GAIN_ANALYSIS_ERROR;
    }

    sampleWindow = (int) ceil(samplefreq * RMS_WINDOW_TIME);

    lsum = 0.;
    rsum = 0.;
    totsamp = 0;

    memset(A, 0, sizeof(A));

    return INIT_GAIN_ANALYSIS_OK;
}

int
GainAnalyzer::InitGainAnalysis(int samplefreq) {
    *this = {};
    if (ResetSampleFrequency(samplefreq) != INIT_GAIN_ANALYSIS_OK) {
        return INIT_GAIN_ANALYSIS_ERROR;
    }

    linpre = linprebuf + MAX_ORDER;
    rinpre = rinprebuf + MAX_ORDER;
    lstep = lstepbuf + MAX_ORDER;
    rstep = rstepbuf + MAX_ORDER;
    lout = loutbuf + MAX_ORDER;
    rout = routbuf + MAX_ORDER;

    memset(B, 0, sizeof(B));

    return INIT_GAIN_ANALYSIS_OK;
}

// returns GAIN_ANALYSIS_OK if successful, GAIN_ANALYSIS_ERROR if not

static __inline double fsqr(const double d) {
    return d * d;
}

int
GainAnalyzer::AnalyzeSamples(const Float_t *left_samples, const Float_t *right_samples, size_t num_samples, int num_channels) {
    const Float_t *curleft;
    const Float_t *curright;
    int64_t batchsamples;
    int64_t cursamples;
    int64_t cursamplepos;
    int i;

    if (num_samples == 0)
        return GAIN_ANALYSIS_OK;

    cursamplepos = 0;
    batchsamples = (int64_t) num_samples;

    switch (num_channels) {
        case 1:
            right_samples = left_samples;
        case 2:
            break;
        default:
            return GAIN_ANALYSIS_ERROR;
    }

    if (num_samples < MAX_ORDER) {
        memcpy(linprebuf + MAX_ORDER, left_samples, num_samples * sizeof(Float_t));
        memcpy(rinprebuf + MAX_ORDER, right_samples, num_samples * sizeof(Float_t));
    } else {
        memcpy(linprebuf + MAX_ORDER, left_samples, MAX_ORDER * sizeof(Float_t));
        memcpy(rinprebuf + MAX_ORDER, right_samples, MAX_ORDER * sizeof(Float_t));
    }

    while (batchsamples > 0) {
        cursamples = batchsamples > sampleWindow - totsamp ? sampleWindow - totsamp : batchsamples;
        if (cursamplepos < MAX_ORDER) {
            curleft = linpre + cursamplepos;
            curright = rinpre + cursamplepos;
            if (cursamples > MAX_ORDER - cursamplepos)
                cursamples = MAX_ORDER - cursamplepos;
        } else {
            curleft = left_samples + cursamplepos;
            curright = right_samples + cursamplepos;
        }

        filterYule(curleft, lstep + totsamp, cursamples, ABYule[freqindex]);
        filterYule(curright, rstep + totsamp, cursamples, ABYule[freqindex]);

        filterButter(lstep + totsamp, lout + totsamp, cursamples, ABButter[freqindex]);
        filterButter(rstep + totsamp, rout + totsamp, cursamples, ABButter[freqindex]);

        curleft = lout + totsamp;                   // Get the squared values
        curright = rout + totsamp;

        i = cursamples % 16;
        while (i--) {
            lsum += fsqr(*curleft++);
            rsum += fsqr(*curright++);
        }
        i = cursamples / 16;
        while (i--) {
            lsum += fsqr(curleft[0])
                    + fsqr(curleft[1])
                    + fsqr(curleft[2])
                    + fsqr(curleft[3])
                    + fsqr(curleft[4])
                    + fsqr(curleft[5])
                    + fsqr(curleft[6])
                    + fsqr(curleft[7])
                    + fsqr(curleft[8])
                    + fsqr(curleft[9])
                    + fsqr(curleft[10])
                    + fsqr(curleft[11])
                    + fsqr(curleft[12])
                    + fsqr(curleft[13])
                    + fsqr(curleft[14])
                    + fsqr(curleft[15]);
            curleft += 16;
            rsum += fsqr(curright[0])
                    + fsqr(curright[1])
                    + fsqr(curright[2])
                    + fsqr(curright[3])
                    + fsqr(curright[4])
                    + fsqr(curright[5])
                    + fsqr(curright[6])
                    + fsqr(curright[7])
                    + fsqr(curright[8])
                    + fsqr(curright[9])
                    + fsqr(curright[10])
                    + fsqr(curright[11])
                    + fsqr(curright[12])
                    + fsqr(curright[13])
                    + fsqr(curright[14])
                    + fsqr(curright[15]);
            curright += 16;
        }

        batchsamples -= cursamples;
        cursamplepos += cursamples;
        totsamp += cursamples;
        if (totsamp == sampleWindow) {  // Get the Root Mean Square (RMS) for this set of samples
            double val = STEPS_per_dB * 10. * log10((lsum + rsum) / totsamp * 0.5 + 1.e-37);
            int ival = (int) val;
            if (ival < 0) ival = 0;
            if (ival >= (int) (sizeof(A) / sizeof(*A))) ival = sizeof(A) / sizeof(*A) - 1;
            A[ival]++;
            lsum = rsum = 0.;
            memmove(loutbuf, loutbuf + totsamp, MAX_ORDER * sizeof(Float_t));
            memmove(routbuf, routbuf + totsamp, MAX_ORDER * sizeof(Float_t));
            memmove(lstepbuf, lstepbuf + totsamp, MAX_ORDER * sizeof(Float_t));
            memmove(rstepbuf, rstepbuf + totsamp, MAX_ORDER * sizeof(Float_t));
            totsamp = 0;
        }
        if (totsamp >
            sampleWindow)   // somehow I really screwed up: Error in programming! Contact author about totsamp > sampleWindow
            return GAIN_ANALYSIS_ERROR;
    }
    if (num_samples < MAX_ORDER) {
        memmove(linprebuf, linprebuf + num_samples, (MAX_ORDER - num_samples) * sizeof(Float_t));
        memmove(rinprebuf, rinprebuf + num_samples, (MAX_ORDER - num_samples) * sizeof(Float_t));
        memcpy(linprebuf + MAX_ORDER - num_samples, left_samples, num_samples * sizeof(Float_t));
        memcpy(rinprebuf + MAX_ORDER - num_samples, right_samples, num_samples * sizeof(Float_t));
    } else {
        memcpy(linprebuf, left_samples + num_samples - MAX_ORDER, MAX_ORDER * sizeof(Float_t));
        memcpy(rinprebuf, right_samples + num_samples - MAX_ORDER, MAX_ORDER * sizeof(Float_t));
    }

    return GAIN_ANALYSIS_OK;
}


Float_t
GainAnalyzer::analyzeResult(const  unsigned int  *Array, size_t len) {
    unsigned int  elems;
    signed int upper;
    size_t i;

    elems = 0;
    for (i = 0; i < len; i++)
        elems += Array[i];
    if (elems == 0)
        return GAIN_NOT_ENOUGH_SAMPLES;

    upper = (signed int) ceil(elems * (1. - RMS_PERCENTILE));
    for (i = len; i-- > 0;) {
        if ((upper -= Array[i]) <= 0)
            break;
    }

    return (Float_t) ((Float_t) PINK_REF - (Float_t) i / (Float_t) STEPS_per_dB);
}


Float_t
GainAnalyzer::GetTitleGain(void) {
    Float_t retval;
    int i;

    retval = analyzeResult(A, sizeof(A) / sizeof(*A));

    for (i = 0; i < (int) (sizeof(A) / sizeof(*A)); i++) {
        B[i] += A[i];
        A[i] = 0;
    }

    for (i = 0; i < MAX_ORDER; i++)
        linprebuf[i] = lstepbuf[i] = loutbuf[i] = rinprebuf[i] = rstepbuf[i] = routbuf[i] = 0.f;

    totsamp = 0;
    lsum = rsum = 0.;
    return retval;
}


Float_t
GainAnalyzer::GetAlbumGain(void) {
    return analyzeResult(B, sizeof(B) / sizeof(*B));
}


/* end of gain_analysis.c */
