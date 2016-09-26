
#pragma once

#include "ssa_int.h"
#include "ssa_float.h"
#include "ssa_vec4f.h"
#include "ssa_bool.h"
#include "ssa_if_block.h"
#include "ssa_phi.h"

template<typename PixelFormat, typename PixelType>
class SSAPixelType : public PixelFormat
{
public:
	SSAPixelType()
	{
	}

	SSAPixelType(SSAInt width, SSAInt height, PixelType pixels)
	: PixelFormat(pixels, width, height), _width(width), _height(height)
	{
		_width32 = SSAVec4i(_width);
		SSAVec4i height32(_height);
		_widthps = SSAVec4f(_width32);
		_heightps = SSAVec4f(height32);
		_width16 = SSAVec8s(_width32, _width32);

		_widthheight = SSAVec4i::shuffle(_width32, height32, 0, 0, 4, 4);
		_widthheightps = SSAVec4i::shuffle(_widthps, _heightps, 0, 0, 4, 4);
	}

	SSAInt width() const { return _width; }
	SSAInt height() const { return _height; }
	SSAInt size() const { return _width * _height; }

	SSABool in_bounds(SSAInt i) const { return i >= 0 && i < _width * _height; }
	SSABool in_bounds(SSAInt x, SSAInt y) const { return x>= 0 && x < _width && y >= 0 && y < _height; }
	//void throw_if_out_of_bounds(SSAInt i) const { if (!in_bounds(i)) throw clan::Exception("Out of bounds"); }
	//void throw_if_out_of_bounds(SSAInt x, SSAInt y) const { if (!in_bounds(x, y)) throw clan::Exception("Out of bounds"); }

	SSAInt s_to_x(SSAFloat s) const { return round(s * SSAFloat(_width)); }
	SSAInt t_to_y(SSAFloat t) const { return round(t * SSAFloat(_height)); }
	SSAInt clamp_x(SSAInt x) const { return clamp(x, _width); }
	SSAInt clamp_y(SSAInt y) const { return clamp(y, _height); }
	SSAInt repeat_x(SSAInt x) const { return repeat(x,_width); }
	SSAInt repeat_y(SSAInt y) const { return repeat(y, _height); }
	SSAInt mirror_x(SSAInt x) const { return mirror(x, _width); }
	SSAInt mirror_y(SSAInt y) const { return mirror(y, _height); }

	static SSAInt int_min(SSAInt a, SSAInt b)
	{
		SSAPhi<SSAInt> phi;
		SSAIfBlock branch;
		branch.if_block(a <= b);
			phi.add_incoming(a);
		branch.else_block();
			phi.add_incoming(b);
		branch.end_block();
		return phi.create();
	}

	static SSAInt int_max(SSAInt a, SSAInt b)
	{
		SSAPhi<SSAInt> phi;
		SSAIfBlock branch;
		branch.if_block(a >= b);
			phi.add_incoming(a);
		branch.else_block();
			phi.add_incoming(b);
		branch.end_block();
		return phi.create();
	}

	static SSAInt clamp(SSAInt v, SSAInt size)
	{
		return int_max(int_min(v, size - 1), 0);
	} 

	static SSAInt repeat(SSAInt v, SSAInt size)
	{
		SSAPhi<SSAInt> phi;
		SSAIfBlock branch;
		branch.if_block(v >= 0);
			phi.add_incoming(v % size);
		branch.else_block();
			phi.add_incoming(size - 1 + v % size);
		branch.end_block();
		return phi.create();
	}

	static SSAInt mirror(SSAInt v, SSAInt size)
	{
		SSAInt size2 = size * 2;
		v = repeat(v, size2);

		SSAPhi<SSAInt> phi;
		SSAIfBlock branch;
		branch.if_block(v < size);
			phi.add_incoming(v);
		branch.else_block();
			phi.add_incoming(size2 - v - 1);
		branch.end_block();
		return phi.create();
	}

	static SSAInt round(SSAFloat v)
	{
		SSAPhi<SSAFloat> phi;
		SSAIfBlock branch;
		branch.if_block(v >= 0.0f);
			phi.add_incoming(v + 0.5f);
		branch.else_block();
			phi.add_incoming(v - 0.5f);
		branch.end_block();
		return SSAInt(phi.create());
	}

	// To do: fix this:
	static SSAInt int_floor(SSAFloat v)
	{
		return SSAInt(v);
	}
	static SSAFloat fract(SSAFloat v) { return v - SSAFloat(int_floor(v)); }

	SSAVec4f get4f(SSAInt x, SSAInt y) const { return PixelFormat::get4f(x + y * _width); }
	void set4f(SSAInt x, SSAInt y, const SSAVec4f &pixel) { PixelFormat::set4f(x + y * _width, pixel); }

	SSAVec4f get_clamp4f(SSAInt x, SSAInt y) const { return get4f(clamp_x(x), clamp_y(y)); }
	SSAVec4f get_repeat4f(SSAInt x, SSAInt y) const { return get4f(repeat_x(x), repeat_y(y)); }
	SSAVec4f get_mirror4f(SSAInt x, SSAInt y) const { return get4f(mirror_x(x), mirror_y(y)); }

	SSAVec4f linear_interpolate4f(SSAFloat s, SSAFloat t, const SSAVec4f *samples) const
	{
		SSAFloat a = fract(s * SSAFloat(_width) - 0.5f);
		SSAFloat b = fract(t * SSAFloat(_height) - 0.5f);
		SSAFloat inv_a = 1.0f - a;
		SSAFloat inv_b = 1.0f - b;
		return
			samples[0] * (inv_a * inv_b) +
			samples[1] * (a * inv_b) +
			samples[2] * (inv_a * b) +
			samples[3] * (a * b);
	}

	void gather_clamp4f(SSAFloat s, SSAFloat t, SSAVec4f *out_pixels) const
	{
		SSAInt x = int_floor(s * SSAFloat(_width) - 0.5f);
		SSAInt y = int_floor(t * SSAFloat(_height) - 0.5f);
		out_pixels[0] = get_clamp4f(x, y);
		out_pixels[1] = get_clamp4f(x + 1, y);
		out_pixels[2] = get_clamp4f(x, y + 1);
		out_pixels[3] = get_clamp4f(x + 1, y + 1);
		/*
		SSAInt x0 = clamp_x(x);
		SSAInt x1 = clamp_x(x + 1);
		SSAInt y0 = clamp_y(y);
		SSAInt y1 = clamp_y(y + 1);
		SSAInt offset0 = y0 * _width;
		SSAInt offset1 = y1 * _width;
		SSAPhi<SSAVec4f> phi0;
		SSAPhi<SSAVec4f> phi1;
		SSAPhi<SSAVec4f> phi2;
		SSAPhi<SSAVec4f> phi3;
		SSAIfBlock if0;
		if0.if_block(x0 + 1 == x1);
			phi0.add_incoming(PixelFormat::get4f(x0 + offset0));
			phi1.add_incoming(PixelFormat::get4f(x1 + offset0));
			phi2.add_incoming(PixelFormat::get4f(x0 + offset1));
			phi3.add_incoming(PixelFormat::get4f(x1 + offset1));
		if0.else_block();
			phi0.add_incoming(PixelFormat::get4f(x0 + offset0));
			phi1.add_incoming(PixelFormat::get4f(x1 + offset0));
			phi2.add_incoming(PixelFormat::get4f(x0 + offset1));
			phi3.add_incoming(PixelFormat::get4f(x1 + offset1));
		if0.end_block();
		out_pixels[0] = phi0.create();
		out_pixels[1] = phi1.create();
		out_pixels[2] = phi2.create();
		out_pixels[3] = phi3.create();
		*/
	}

	void gather_repeat4f(SSAFloat s, SSAFloat t, SSAVec4f *out_pixels) const
	{
		SSAInt x = int_floor(s * SSAFloat(_width) - 0.5f);
		SSAInt y = int_floor(t * SSAFloat(_height) - 0.5f);
		out_pixels[0] = get_repeat4f(x, y);
		out_pixels[1] = get_repeat4f(x + 1, y);
		out_pixels[2] = get_repeat4f(x, y + 1);
		out_pixels[3] = get_repeat4f(x + 1, y + 1);
	}

	void gather_mirror4f(SSAFloat s, SSAFloat t, SSAVec4f *out_pixels) const
	{
		SSAInt x = int_floor(s * SSAFloat(_width) - 0.5f);
		SSAInt y = int_floor(t * SSAFloat(_height) - 0.5f);
		out_pixels[0] = get_mirror4f(x, y);
		out_pixels[1] = get_mirror4f(x + 1, y);
		out_pixels[2] = get_mirror4f(x, y + 1);
		out_pixels[3] = get_mirror4f(x + 1, y + 1);
	}

	SSAVec4f nearest_clamp4f(SSAFloat s, SSAFloat t) const { return get_clamp4f(s_to_x(s), t_to_y(t)); }
	SSAVec4f nearest_repeat4f(SSAFloat s, SSAFloat t) const { return get_repeat4f(s_to_x(s), t_to_y(t)); }
	SSAVec4f nearest_mirror4f(SSAFloat s, SSAFloat t) const { return get_mirror4f(s_to_x(s), t_to_y(t)); }

	SSAVec4f linear_clamp4f(SSAFloat s, SSAFloat t) const
	{
		SSAVec4f samples[4];
		gather_clamp4f(s, t, samples);
		return linear_interpolate4f(s, t, samples);
	}

	SSAVec4f linear_repeat4f(SSAFloat s, SSAFloat t) const
	{
		SSAVec4f samples[4];
		gather_repeat4f(s, t, samples);
		return linear_interpolate4f(s, t, samples);
	}

	SSAVec4f linear_mirror4f(SSAFloat s, SSAFloat t) const
	{
		SSAVec4f samples[4];
		gather_mirror4f(s, t, samples);
		return linear_interpolate4f(s, t, samples);
	}

	/////////////////////////////////////////////////////////////////////////
	// Packed versions:

	SSAVec4i s_to_x(SSAVec4f s) const { return round(s * SSAVec4f(_width)); }
	SSAVec4i t_to_y(SSAVec4f t) const { return round(t * SSAVec4f(_height)); }
	SSAVec4i clamp_x(SSAVec4i x) const { return clamp(x, _width); }
	SSAVec4i clamp_y(SSAVec4i y) const { return clamp(y, _height); }
	SSAVec4i repeat_x(SSAVec4i x) const { return repeat(x,_width); }
	SSAVec4i repeat_y(SSAVec4i y) const { return repeat(y, _height); }
	SSAVec4i mirror_x(SSAVec4i x) const { return mirror(x, _width); }
	SSAVec4i mirror_y(SSAVec4i y) const { return mirror(y, _height); }

	static SSAVec4i clamp(SSAVec4i v, SSAInt size)
	{
		return SSAVec4i::max_sse41(SSAVec4i::min_sse41(v, size - 1), 0);
	} 

	static SSAVec4i repeat(SSAVec4i v, SSAInt size)
	{
		return clamp(v, size);
		/*SSAPhi<SSAInt> phi;
		SSAIfBlock branch;
		branch.if_block(v >= 0);
			phi.add_incoming(v % size);
		branch.else_block();
			phi.add_incoming(size - 1 + v % size);
		branch.end_block();
		return phi.create();*/
	}

	static SSAVec4i mirror(SSAVec4i v, SSAInt size)
	{
		return clamp(v, size);
		/*SSAInt size2 = size * 2;
		v = repeat(v, size2);

		SSAPhi<SSAInt> phi;
		SSAIfBlock branch;
		branch.if_block(v < size);
			phi.add_incoming(v);
		branch.else_block();
			phi.add_incoming(size2 - v - 1);
		branch.end_block();
		return phi.create();*/
	}

	static SSAVec4i round(SSAVec4f v)
	{
		// Maybe we should use the normal round SSE function (but that requires the rounding mode is set the round to nearest before the code runs)
		SSAVec4i signbit = (SSAVec4i::bitcast(v) & 0x80000000);
		SSAVec4f signed_half = SSAVec4f::bitcast(signbit | SSAVec4i::bitcast(SSAVec4f(0.5f)));
		return v + signed_half;
	}

	static SSAVec4i int_floor(SSAVec4f v)
	{
		return SSAVec4i(v) - (SSAVec4i::bitcast(v) >> 31);
	}

	static SSAVec4f fract(SSAVec4f v)
	{
		// return v - SSAVec4f::floor_sse4(v);
		return v - SSAVec4f(int_floor(v));
	}

	template<typename WrapXFunctor, typename WrapYFunctor>
	SSAVec4f nearest_helper4f(SSAVec4f s, SSAVec4f t, int index, WrapXFunctor wrap_x, WrapYFunctor wrap_y) const
	{
		SSAVec4i x = int_floor(s * _widthps - 0.5f);
		SSAVec4i y = int_floor(t * _heightps - 0.5f);
		SSAVec8s y16 = SSAVec8s(wrap_y(y), wrap_y(y));
		SSAVec8s offsethi = SSAVec8s::mulhi(y16, _width16);
		SSAVec8s offsetlo = y16 * _width16;
		SSAVec4i offset = SSAVec4i::combinelo(offsetlo, offsethi) + x;
		return PixelFormat::get4f(offset[index]);
	}

	SSAVec4f nearest_clamp4f(SSAVec4f s, SSAVec4f t, int index) const
	{
		struct WrapX { WrapX(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->clamp_x(v); } const SSAPixelType *self; };
		struct WrapY { WrapY(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->clamp_y(v); } const SSAPixelType *self; };
		return nearest_helper4f(s, t, index, WrapX(this), WrapY(this));
		/*
		return nearest_helper4f(
			s, t, index,
			[this](SSAVec4i v) -> SSAVec4i { return clamp_x(v); },
			[this](SSAVec4i v) -> SSAVec4i { return clamp_y(v); });
		*/
	}

	SSAVec4f nearest_repeat4f(SSAVec4f s, SSAVec4f t, int index) const
	{
		struct WrapX { WrapX(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->repeat_x(v); } const SSAPixelType *self; };
		struct WrapY { WrapY(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->repeat_y(v); } const SSAPixelType *self; };
		return nearest_helper4f(s, t, index, WrapX(this), WrapY(this));
		/*
		return nearest_helper4f(
			s, t, index,
			[this](SSAVec4i v) -> SSAVec4i { return repeat_x(v); },
			[this](SSAVec4i v) -> SSAVec4i { return repeat_y(v); });
		*/
	}

	SSAVec4f nearest_mirror4f(SSAVec4f s, SSAVec4f t, int index) const
	{
		struct WrapX { WrapX(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->mirror_x(v); } const SSAPixelType *self; };
		struct WrapY { WrapY(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->mirror_y(v); } const SSAPixelType *self; };
		return nearest_helper4f(s, t, index, WrapX(this), WrapY(this));
		/*
		return nearest_helper4f(
			s, t, index,
			[this](SSAVec4i v) -> SSAVec4i { return mirror_x(v); },
			[this](SSAVec4i v) -> SSAVec4i { return mirror_y(v); });
		*/
	}

	template<typename WrapXFunctor, typename WrapYFunctor>
	void gather_helper4f(SSAVec4f s, SSAVec4f t, int index, SSAVec4f *out_pixels, WrapXFunctor wrap_x, WrapYFunctor wrap_y) const
	{
		SSAVec4i x = int_floor(s * _widthps - 0.5f);
		SSAVec4i y = int_floor(t * _heightps - 0.5f);
		SSAVec8s y16 = SSAVec8s(wrap_y(y + 1), wrap_y(y));
		SSAVec8s offsethi = SSAVec8s::mulhi(y16, _width16);
		SSAVec8s offsetlo = y16 * _width16;
		SSAVec4i x0 = wrap_x(x);
		SSAVec4i x1 = wrap_x(x + 1);
		SSAVec4i line0 = SSAVec4i::combinehi(offsetlo, offsethi);
		SSAVec4i line1 = SSAVec4i::combinelo(offsetlo, offsethi);
		SSAVec4i offset0 = x0 + line0;
		SSAVec4i offset1 = x1 + line0;
		SSAVec4i offset2 = x0 + line1;
		SSAVec4i offset3 = x1 + line1;
		out_pixels[0] = PixelFormat::get4f(offset0[index]);
		out_pixels[1] = PixelFormat::get4f(offset1[index]);
		out_pixels[2] = PixelFormat::get4f(offset2[index]);
		out_pixels[3] = PixelFormat::get4f(offset3[index]);
	}

	void gather_clamp4f(SSAVec4f s, SSAVec4f t, int index, SSAVec4f *out_pixels) const
	{
		struct WrapX { WrapX(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->clamp_x(v); } const SSAPixelType *self; };
		struct WrapY { WrapY(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->clamp_y(v); } const SSAPixelType *self; };
		return gather_helper4f(s, t, index, out_pixels, WrapX(this), WrapY(this));
		/*
		gather_helper4f(
			s, t, index, out_pixels,
			[this](SSAVec4i v) -> SSAVec4i { return clamp_x(v); },
			[this](SSAVec4i v) -> SSAVec4i { return clamp_y(v); });
		*/
	}

	void gather_repeat4f(SSAVec4f s, SSAVec4f t, int index, SSAVec4f *out_pixels) const
	{
		struct WrapX { WrapX(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->repeat_x(v); } const SSAPixelType *self; };
		struct WrapY { WrapY(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->repeat_y(v); } const SSAPixelType *self; };
		return gather_helper4f(s, t, index, out_pixels, WrapX(this), WrapY(this));
		/*
		gather_helper4f(
			s, t, index, out_pixels,
			[this](SSAVec4i v) -> SSAVec4i { return repeat_x(v); },
			[this](SSAVec4i v) -> SSAVec4i { return repeat_y(v); });
		*/
	}

	void gather_mirror4f(SSAVec4f s, SSAVec4f t, int index, SSAVec4f *out_pixels) const
	{
		struct WrapX { WrapX(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->mirror_x(v); } const SSAPixelType *self; };
		struct WrapY { WrapY(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i v) { return self->mirror_y(v); } const SSAPixelType *self; };
		return gather_helper4f(s, t, index, out_pixels, WrapX(this), WrapY(this));
		/*
		gather_helper4f(
			s, t, index, out_pixels,
			[this](SSAVec4i v) -> SSAVec4i { return mirror_x(v); },
			[this](SSAVec4i v) -> SSAVec4i { return mirror_y(v); });
		*/
	}

	SSAVec4f linear_clamp4f(SSAVec4f s, SSAVec4f t, int index) const
	{
		SSAScopeHint hint("linearclamp");
		SSAVec4f samples[4];
		gather_clamp4f(s, t, index, samples);
		return linear_interpolate4f(s, t, index, samples);
	}

	SSAVec4f linear_repeat4f(SSAVec4f s, SSAVec4f t, int index) const
	{
		SSAVec4f samples[4];
		gather_repeat4f(s, t, index, samples);
		return linear_interpolate4f(s, t, index, samples);
	}

	SSAVec4f linear_mirror4f(SSAVec4f s, SSAVec4f t, int index) const
	{
		SSAVec4f samples[4];
		gather_mirror4f(s, t, index, samples);
		return linear_interpolate4f(s, t, index, samples);
	}

	SSAVec4f linear_interpolate4f(SSAVec4f s, SSAVec4f t, int index, const SSAVec4f *samples) const
	{
		SSAVec4f a = fract(s * _widthps - 0.5f);
		SSAVec4f b = fract(t * _heightps - 0.5f);
		SSAVec4f inv_a = 1.0f - a;
		SSAVec4f inv_b = 1.0f - b;
		return
			samples[0] * SSAVec4f::shuffle(inv_a * inv_b, index, index, index, index) +
			samples[1] * SSAVec4f::shuffle(a * inv_b, index, index, index, index) +
			samples[2] * SSAVec4f::shuffle(inv_a * b, index, index, index, index) +
			samples[3] * SSAVec4f::shuffle(a * b, index, index, index, index);
	}

	/////////////////////////////////////////////////////////////////////////

	SSAVec4i clamp(SSAVec4i sstt) const
	{
		return SSAVec4i::max_sse41(SSAVec4i::min_sse41(sstt, _widthheight - 1), 0);
	} 

	template<typename WrapFunctor>
	void gather_helper4f(SSAVec4f st, SSAVec4f *out_pixels, WrapFunctor wrap) const
	{
		SSAVec4f sstt = SSAVec4f::shuffle(st, 0, 0, 1, 1);
		SSAVec4i xxyy = wrap(int_floor(sstt * _widthheightps - 0.5f) + SSAVec4i(0, 1, 0, 1));
		SSAVec4i xxoffset = SSAVec4f::shuffle(xxyy, xxyy * _width32, 0, 1, 6, 7);
		SSAVec4i offsets = SSAVec4i::shuffle(xxoffset, 0, 1, 0, 1) + SSAVec4i::shuffle(xxoffset, 2, 2, 3, 3);
		out_pixels[0] = PixelFormat::get4f(offsets[0]);
		out_pixels[1] = PixelFormat::get4f(offsets[1]);
		out_pixels[2] = PixelFormat::get4f(offsets[2]);
		out_pixels[3] = PixelFormat::get4f(offsets[3]);
	}

	void gather_clamp4f(SSAVec4f st, SSAVec4f *out_pixels) const
	{
		struct Wrap { Wrap(const SSAPixelType *self) : self(self) { } SSAVec4i operator()(SSAVec4i sstt) { return self->clamp(sstt); } const SSAPixelType *self; };
		return gather_helper4f(st, out_pixels, Wrap(this));
	}

	SSAVec4f linear_clamp4f(SSAVec4f st) const
	{
		SSAScopeHint hint("linearclamp");
		SSAVec4f samples[4];
		gather_clamp4f(st, samples);
		return linear_interpolate4f(st, samples);
	}

	SSAVec4f linear_interpolate4f(SSAVec4f st, const SSAVec4f *samples) const
	{
		SSAVec4f sstt = SSAVec4f::shuffle(st, 0, 0, 1, 1);
		SSAVec4f aabb = fract(sstt * _widthheightps - 0.5f);
		SSAVec4f inv_aabb = 1.0f - aabb;
		SSAVec4f ab_inv_ab = SSAVec4f::shuffle(aabb, inv_aabb, 0, 2, 4, 6);
		SSAVec4f ab__inv_a_b__inv_a_inv_b__a_invb = ab_inv_ab * SSAVec4f::shuffle(ab_inv_ab, 1, 2, 3, 0);
		return
			samples[0] * SSAVec4f::shuffle(ab__inv_a_b__inv_a_inv_b__a_invb, 2, 2, 2, 2) +
			samples[1] * SSAVec4f::shuffle(ab__inv_a_b__inv_a_inv_b__a_invb, 3, 3, 3, 3) +
			samples[2] * SSAVec4f::shuffle(ab__inv_a_b__inv_a_inv_b__a_invb, 1, 1, 1, 1) +
			samples[3] * SSAVec4f::shuffle(ab__inv_a_b__inv_a_inv_b__a_invb, 0, 0, 0, 0);
	}

public:
	SSAInt _width;
	SSAInt _height;
	SSAVec4i _width32;
	SSAVec8s _width16;
	SSAVec4f _widthps;
	SSAVec4f _heightps;

	SSAVec4i _widthheight;
	SSAVec4f _widthheightps;
};
