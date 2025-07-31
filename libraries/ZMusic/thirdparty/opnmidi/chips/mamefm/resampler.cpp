// SPDX-License-Identifier: GPL-2.0-only
#include "resampler.hpp"

enum Stereo
{
	LEFT = 0,
	RIGHT = 1
};

static const size_t SMPL_BUF_SIZE_ = 0x10000;

namespace chip
{
	AbstractResampler::AbstractResampler()
	{
		for (int pan = LEFT; pan <= RIGHT; ++pan) {
			destBuf_[pan] = new sample[SMPL_BUF_SIZE_]();
		}
	}

	AbstractResampler::~AbstractResampler()
	{
		for (int pan = LEFT; pan <= RIGHT; ++pan) {
			delete[] destBuf_[pan];
		}
	}

	void AbstractResampler::init(int srcRate, int destRate, size_t maxDuration)
	{
		srcRate_ = srcRate;
		maxDuration_ = maxDuration;
		destRate_ = destRate;
		updateRateRatio();
	}

	void AbstractResampler::setDestributionRate(int destRate)
	{
		destRate_ = destRate;
		updateRateRatio();
	}

	void AbstractResampler::setMaxDuration(size_t maxDuration)
	{
		maxDuration_ = maxDuration;
	}

	/****************************************/
	sample** LinearResampler::interpolate(sample** src, size_t nSamples, size_t intrSize)
	{
		(void)intrSize;
		// Linear interplation
		for (int pan = LEFT; pan <= RIGHT; ++pan) {
			for (size_t n = 0; n < nSamples; ++n) {
				float curnf = n * rateRatio_;
				int curni = static_cast<int>(curnf);
				float sub = curnf - curni;
				if (sub) {
					destBuf_[pan][n] = static_cast<sample>(src[pan][curni] + (src[pan][curni + 1] - src[pan][curni]) * sub);
				}
				else /* if (sub == 0) */ {
					destBuf_[pan][n] = src[pan][curni];
				}
			}
		}

		return destBuf_;
	}

	/****************************************/
	const float SincResampler::F_PI_ = 3.14159265f;
	const int SincResampler::SINC_OFFSET_ = 16;

	void SincResampler::init(int srcRate, int destRate, size_t maxDuration)
	{
		AbstractResampler::init(srcRate, destRate, maxDuration);
		initSincTables();
	}

	void SincResampler::setDestributionRate(int destRate)
	{
		AbstractResampler::setDestributionRate(destRate);
		initSincTables();
	}

	void SincResampler::setMaxDuration(size_t maxDuration)
	{
		AbstractResampler::setMaxDuration(maxDuration);
		initSincTables();
	}

	sample** SincResampler::interpolate(sample** src, size_t nSamples, size_t intrSize)
	{
		// Sinc interpolation
		size_t offsetx2 = SINC_OFFSET_ << 1;

		for (int pan = LEFT; pan <= RIGHT; ++pan) {
			for (size_t n = 0; n < nSamples; ++n) {
				size_t seg = n * offsetx2;
				int curn = static_cast<int>(n * rateRatio_);
				int k = curn - SINC_OFFSET_;
				if (k < 0) k = 0;
				int end = curn + SINC_OFFSET_;
				if (static_cast<size_t>(end) > intrSize) end = static_cast<int>(intrSize);
				sample samp = 0;
				for (; k < end; ++k) {
					samp += static_cast<sample>(src[pan][k] * sincTable_[seg + SINC_OFFSET_ + (k - curn)]);
				}
				destBuf_[pan][n] = samp;
			}
		}

		return destBuf_;
	}

	void SincResampler::initSincTables()
	{
		size_t maxSamples = destRate_ * maxDuration_ / 1000;

		if (srcRate_ != destRate_) {
			size_t intrSize = calculateInternalSampleSize(maxSamples);
			size_t offsetx2 = SINC_OFFSET_ << 1;
			sincTable_.resize(maxSamples * offsetx2);
			for (size_t n = 0; n < maxSamples; ++n) {
				size_t seg = n * offsetx2;
				float rcurn = n * rateRatio_;
				int curn = static_cast<int>(rcurn);
				int k = curn - SINC_OFFSET_;
				if (k < 0) k = 0;
				int end = curn + SINC_OFFSET_;
				if (static_cast<size_t>(end) > intrSize) end = static_cast<int>(intrSize);
				for (; k < end; ++k) {
					sincTable_[seg + SINC_OFFSET_ + (k - curn)] = sinc(F_PI_ * (rcurn - k));
				}
			}
		}
	}
}
