
#include "core/pathfill.h"
#include <cmath>
#include <algorithm>

static const int AntialiasLevel = 8;
static const int MaskBlockSize = 16;
static const int MaskBufferSize = MaskBlockSize * MaskBlockSize;
static const int ScanlineBlockSize = MaskBlockSize * AntialiasLevel;

class PathScanlineEdge
{
public:
	PathScanlineEdge() = default;
	PathScanlineEdge(double x, bool up_direction) : x(x), up_direction(up_direction) { }

	double x = 0.0;
	bool up_direction = false;
};

class PathScanline
{
public:
	std::vector<PathScanlineEdge> edges;
	std::vector<unsigned char> pixels;

	void insert_sorted(PathScanlineEdge edge)
	{
		edges.push_back(edge);

		for (size_t pos = edges.size() - 1; pos > 0 && edges[pos - 1].x >= edge.x; pos--)
		{
			PathScanlineEdge temp = edges[pos - 1];
			edges[pos - 1] = edges[pos];
			edges[pos] = temp;
		}
	}
};

class PathRasterRange
{
public:
	void Begin(const PathScanline* scanline, PathFillMode mode);
	void Next();

	bool found = false;
	int x0;
	int x1;

private:
	const PathScanline* scanline = nullptr;
	PathFillMode mode;
	size_t i = 0;
	int nonzero_rule = 0;
};

enum class PathFillBlockResult
{
	Empty,
	Partial,
	Full
};

class PathMaskBuffer
{
public:
	void BeginRow(PathScanline* scanlines, PathFillMode mode);
	PathFillBlockResult FillBlock(int xpos);

	unsigned char MaskBufferData[MaskBufferSize];

private:
	bool IsFullBlock(int xpos) const;

	PathRasterRange Range[ScanlineBlockSize];
};

class PathFillRasterizer
{
public:
	void Rasterize(const PathFillDesc& path, uint8_t* dest, int width, int height);

private:
	void Clear();

	void Begin(double x, double y);
	void QuadraticBezier(double cp1_x, double cp1_y, double cp2_x, double cp2_y);
	void CubicBezier(double cp1_x, double cp1_y, double cp2_x, double cp2_y, double cp3_x, double cp3_y);
	void Line(double x, double y);
	void End(bool close);

	void SubdivideBezier(int level, double cp0_x, double cp0_y, double cp1_x, double cp1_y, double cp2_x, double cp2_y, double cp3_x, double cp3_y, double t0, double t1);
	static Point PointOnBezier(double cp0_x, double cp0_y, double cp1_x, double cp1_y, double cp2_x, double cp2_y, double cp3_x, double cp3_y, double t);

	void Fill(PathFillMode mode, uint8_t* dest, int dest_width, int dest_height);

	struct Extent
	{
		Extent() : left(INT_MAX), right(0) {}
		int left;
		int right;
	};

	Extent FindExtent(const PathScanline* scanline, int max_width);

	double start_x = 0.0;
	double start_y = 0.0;
	double last_x = 0.0;
	double last_y = 0.0;

	int first_scanline = 0;
	int last_scanline = 0;

	int width = 0;
	int height = 0;
	std::vector<PathScanline> scanlines;

	PathMaskBuffer mask_blocks;
};

/////////////////////////////////////////////////////////////////////////////

void PathFillDesc::Rasterize(uint8_t* dest, int width, int height, bool blend)
{
	if (!blend)
	{
		memset(dest, 0, width * height);
	}
	PathFillRasterizer rasterizer;
	rasterizer.Rasterize(*this, dest, width, height);
}

/////////////////////////////////////////////////////////////////////////////

void PathFillRasterizer::Rasterize(const PathFillDesc& path, uint8_t* dest, int dest_width, int dest_height)
{
	Clear();

	// For simplicity of the code, ensure the mask is always a multiple of MaskBlockSize
	int block_width = ScanlineBlockSize * ((dest_width + MaskBlockSize - 1) / MaskBlockSize);
	int block_height = ScanlineBlockSize * ((dest_height + MaskBlockSize - 1) / MaskBlockSize);

	if (width != block_width || height != block_height)
	{
		width = block_width;
		height = block_height;

		scanlines.resize(block_height);
		first_scanline = scanlines.size();
		last_scanline = 0;
	}

	for (const auto& subpath : path.subpaths)
	{
		Point start_point = subpath.points[0];
		Begin(start_point.x, start_point.y);

		size_t i = 1;
		for (PathFillCommand command : subpath.commands)
		{
			if (command == PathFillCommand::line)
			{
				const Point& next_point = subpath.points[i];
				i++;

				Line(next_point.x, next_point.y);
			}
			else if (command == PathFillCommand::quadradic)
			{
				const Point& control = subpath.points[i];
				const Point& next_point = subpath.points[i + 1];
				i += 2;

				QuadraticBezier(control.x, control.y, next_point.x, next_point.y);
			}
			else if (command == PathFillCommand::cubic)
			{
				const Point& control1 = subpath.points[i];
				const Point& control2 = subpath.points[i + 1];
				const Point& next_point = subpath.points[i + 2];
				i += 3;

				CubicBezier(control1.x, control1.y, control2.x, control2.y, next_point.x, next_point.y);
			}
		}

		End(subpath.closed);
	}

	Fill(path.fill_mode, dest, dest_width, dest_height);
}

void PathFillRasterizer::Fill(PathFillMode mode, uint8_t* dest, int dest_width, int dest_height)
{
	if (scanlines.empty()) return;

	int start_y = first_scanline / ScanlineBlockSize * ScanlineBlockSize;
	int end_y = (last_scanline + ScanlineBlockSize - 1) / ScanlineBlockSize * ScanlineBlockSize;

	for (int ypos = start_y; ypos < end_y; ypos += ScanlineBlockSize)
	{
		mask_blocks.BeginRow(&scanlines[ypos], mode);

		Extent extent = FindExtent(&scanlines[ypos], width);
		for (int xpos = extent.left; xpos < extent.right; xpos += ScanlineBlockSize)
		{
			PathFillBlockResult result = mask_blocks.FillBlock(xpos);

			int dest_x = xpos / AntialiasLevel;
			int dest_y = ypos / AntialiasLevel;
			int count_x = std::min(dest_x + MaskBlockSize, dest_width) - dest_x;
			int count_y = std::min(dest_y + MaskBlockSize, dest_height) - dest_y;

			if (result == PathFillBlockResult::Full)
			{
				for (int i = 0; i < count_y; i++)
				{
					uint8_t* dline = dest + dest_x + (dest_y + i) * dest_width;
					memset(dline, 255, count_x);
				}
			}
			else if (result == PathFillBlockResult::Partial)
			{
				for (int i = 0; i < count_y; i++)
				{
					const uint8_t* sline = mask_blocks.MaskBufferData + i * MaskBlockSize;
					uint8_t* dline = dest + dest_x + (dest_y + i) * dest_width;
					for (int j = 0; j < count_x; j++)
					{
						dline[j] = std::min((int)dline[j] + (int)sline[j], 255);
					}
				}
			}
		}
	}
}

PathFillRasterizer::Extent PathFillRasterizer::FindExtent(const PathScanline* scanline, int max_width)
{
	// Find scanline extents
	Extent extent;
	for (unsigned int cnt = 0; cnt < ScanlineBlockSize; cnt++, scanline++)
	{
		if (scanline->edges.empty())
			continue;

		extent.left = std::min(extent.left, (int)scanline->edges.front().x);
		extent.right = std::max(extent.right, (int)scanline->edges.back().x);
	}
	extent.left = std::max(extent.left, 0);
	extent.right = std::min(extent.right, max_width);

	return extent;
}

void PathFillRasterizer::Clear()
{
	for (size_t y = first_scanline; y < last_scanline; y++)
	{
		auto& scanline = scanlines[y];
		if (!scanline.edges.empty())
		{
			scanline.edges.clear();
		}
	}

	first_scanline = scanlines.size();
	last_scanline = 0;
}

void PathFillRasterizer::Begin(double x, double y)
{
	start_x = last_x = x;
	start_y = last_y = y;
}

void PathFillRasterizer::End(bool close)
{
	if (close)
	{
		Line(start_x, start_y);
	}
}

void PathFillRasterizer::QuadraticBezier(double qcp1_x, double qcp1_y, double qcp2_x, double qcp2_y)
{
	double qcp0_x = last_x;
	double qcp0_y = last_y;

	// Convert to cubic:
	double cp1_x = qcp0_x + 2.0 * (qcp1_x - qcp0_x) / 3.0;
	double cp1_y = qcp0_y + 2.0 * (qcp1_y - qcp0_y) / 3.0;
	double cp2_x = qcp1_x + (qcp2_x - qcp1_x) / 3.0;
	double cp2_y = qcp1_y + (qcp2_y - qcp1_y) / 3.0;
	double cp3_x = qcp2_x;
	double cp3_y = qcp2_y;
	CubicBezier(cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y);
}

void PathFillRasterizer::CubicBezier(double cp1_x, double cp1_y, double cp2_x, double cp2_y, double cp3_x, double cp3_y)
{
	double cp0_x = last_x;
	double cp0_y = last_y;

	double estimated_length =
		std::sqrt((cp1_x - cp0_x) * (cp1_x - cp0_x) + (cp1_y - cp0_y) * (cp1_y - cp0_y)) +
		std::sqrt((cp1_x - cp0_x) * (cp1_x - cp0_x) + (cp1_y - cp0_y) * (cp1_y - cp0_y)) +
		std::sqrt((cp1_x - cp0_x) * (cp1_x - cp0_x) + (cp1_y - cp0_y) * (cp1_y - cp0_y));

	double min_segs = 10.0;
	double segs = estimated_length / 5.0;
	int steps = (int)std::ceil(std::sqrt(segs * segs * 0.3f + min_segs));
	for (int i = 0; i < steps; i++)
	{
		//Point sp = PointOnBezier(cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, i / (double)steps);
		Point ep = PointOnBezier(cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, (i + 1) / (double)steps);
		Line(ep.x, ep.y);
	}

	// http://ciechanowski.me/blog/2014/02/18/drawing-bezier-curves/
	// http://antigrain.com/research/adaptive_bezier/  (best method, unfortunately GPL example code)
}

void PathFillRasterizer::SubdivideBezier(int level, double cp0_x, double cp0_y, double cp1_x, double cp1_y, double cp2_x, double cp2_y, double cp3_x, double cp3_y, double t0, double t1)
{
	const double split_angle_cos = 0.99f;

	double tc = (t0 + t1) * 0.5f;

	Point sp = PointOnBezier(cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, t0);
	Point cp = PointOnBezier(cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, tc);
	Point ep = PointOnBezier(cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, t1);

	Point sp2cp(cp.x - sp.x, cp.y - sp.y);
	Point cp2ep(ep.x - cp.x, ep.y - cp.y);

	// Normalize
	double len_sp2cp = std::sqrt(sp2cp.x * sp2cp.x + sp2cp.y * sp2cp.y);
	double len_cp2ep = std::sqrt(cp2ep.x * cp2ep.x + cp2ep.y * cp2ep.y);
	if (len_sp2cp > 0.0) { sp2cp.x /= len_sp2cp; sp2cp.y /= len_sp2cp; }
	if (len_cp2ep > 0.0) { cp2ep.x /= len_cp2ep; cp2ep.y /= len_cp2ep; }

	double dot = sp2cp.x * cp2ep.x + sp2cp.y * cp2ep.y;
	if (dot < split_angle_cos && level < 15)
	{
		SubdivideBezier(level + 1, cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, t0, tc);
		SubdivideBezier(level + 1, cp0_x, cp0_y, cp1_x, cp1_y, cp2_x, cp2_y, cp3_x, cp3_y, tc, t1);
	}
	else
	{
		Line(ep.x, ep.y);
	}
}

Point PathFillRasterizer::PointOnBezier(double cp0_x, double cp0_y, double cp1_x, double cp1_y, double cp2_x, double cp2_y, double cp3_x, double cp3_y, double t)
{
	const int num_cp = 4;

	double cp_x[4] = { cp0_x, cp1_x, cp2_x, cp3_x };
	double cp_y[4] = { cp0_y, cp1_y, cp2_y, cp3_y };

	// Perform deCasteljau iterations:
	// (linear interpolate between the control points)
	double a = 1.0 - t;
	double b = t;
	for (int j = num_cp - 1; j > 0; j--)
	{
		for (int i = 0; i < j; i++)
		{
			cp_x[i] = a * cp_x[i] + b * cp_x[i + 1];
			cp_y[i] = a * cp_y[i] + b * cp_y[i + 1];
		}
	}

	return Point(cp_x[0], cp_y[0]);
}

void PathFillRasterizer::Line(double x1, double y1)
{
	double x0 = last_x;
	double y0 = last_y;

	last_x = x1;
	last_y = y1;

	x0 *= static_cast<double>(AntialiasLevel);
	x1 *= static_cast<double>(AntialiasLevel);
	y0 *= static_cast<double>(AntialiasLevel);
	y1 *= static_cast<double>(AntialiasLevel);

	bool up_direction = y1 < y0;
	double dy = y1 - y0;

	constexpr const double epsilon = std::numeric_limits<double>::epsilon();
	if (dy < -epsilon || dy > epsilon)
	{
		int start_y = static_cast<int>(std::floor(std::min(y0, y1) + 0.5f));
		int end_y = static_cast<int>(std::floor(std::max(y0, y1) - 0.5f)) + 1;

		start_y = std::max(start_y, 0);
		end_y = std::min(end_y, height);

		double rcp_dy = 1.0 / dy;

		first_scanline = std::min(first_scanline, start_y);
		last_scanline = std::max(last_scanline, end_y);

		for (int y = start_y; y < end_y; y++)
		{
			double ypos = y + 0.5f;
			double x = x0 + (x1 - x0) * (ypos - y0) * rcp_dy;
			scanlines[y].insert_sorted(PathScanlineEdge(x, up_direction));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void PathRasterRange::Begin(const PathScanline* new_scanline, PathFillMode new_mode)
{
	scanline = new_scanline;
	mode = new_mode;
	found = false;
	i = 0;
	nonzero_rule = 0;
	Next();
}

void PathRasterRange::Next()
{
	if (i + 1 >= scanline->edges.size())
	{
		found = false;
		return;
	}

	if (mode == PathFillMode::alternate)
	{
		x0 = static_cast<int>(scanline->edges[i].x + 0.5f);
		x1 = static_cast<int>(scanline->edges[i + 1].x - 0.5f) + 1;
		i += 2;
		found = true;
	}
	else
	{
		x0 = static_cast<int>(scanline->edges[i].x + 0.5f);
		nonzero_rule += scanline->edges[i].up_direction ? 1 : -1;
		i++;

		while (i < scanline->edges.size())
		{
			nonzero_rule += scanline->edges[i].up_direction ? 1 : -1;
			x1 = static_cast<int>(scanline->edges[i].x - 0.5f) + 1;
			i++;

			if (nonzero_rule == 0)
			{
				found = true;
				return;
			}
		}
		found = false;
	}
}

/////////////////////////////////////////////////////////////////////////

void PathMaskBuffer::BeginRow(PathScanline* scanlines, PathFillMode mode)
{
	for (unsigned int cnt = 0; cnt < ScanlineBlockSize; cnt++)
	{
		Range[cnt].Begin(&scanlines[cnt], mode);
	}
}

#if 0
PathFillBlockResult PathMaskBuffer::FillBlock(int xpos)
{
	if (IsFullBlock(xpos))
	{
		return PathFillBlockResult::Full;
	}

	const int block_size = MaskBlockSize / 16 * MaskBlockSize;
	__m128i block[block_size];

	for (auto& elem : block)
		elem = _mm_setzero_si128();

	for (unsigned int cnt = 0; cnt < ScanlineBlockSize; cnt++)
	{
		__m128i* line = &block[MaskBlockSize / 16 * (cnt / AntialiasLevel)];

		while (range[cnt].found)
		{
			int x0 = range[cnt].x0;
			if (x0 >= xpos + ScanlineBlockSize)
				break;
			int x1 = range[cnt].x1;

			x0 = max(x0, xpos);
			x1 = min(x1, xpos + ScanlineBlockSize);

			if (x0 >= x1)	// Done segment
			{
				range[cnt].next();
			}
			else
			{
				for (int sse_block = 0; sse_block < MaskBlockSize / 16; sse_block++)
				{
					for (int alias_cnt = 0; alias_cnt < (AntialiasLevel); alias_cnt++)
					{
						__m128i start = _mm_set1_epi8((x0 + alias_cnt - xpos) / AntialiasLevel - 16 * sse_block);
						__m128i end = _mm_set1_epi8((x1 + alias_cnt - xpos) / AntialiasLevel - 16 * sse_block);
						__m128i x = _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

						__m128i left = _mm_cmplt_epi8(x, start);
						__m128i right = _mm_cmplt_epi8(x, end);
						__m128i mask = _mm_andnot_si128(left, right);
						__m128i add_value = _mm_and_si128(mask, _mm_set1_epi8(256 / (AntialiasLevel * AntialiasLevel)));

						line[sse_block] = _mm_adds_epu8(line[sse_block], add_value);
					}
				}

				range[cnt].x0 = x1;	// For next time
			}
		}
	}

	__m128i empty_status = _mm_setzero_si128();
	for (auto& elem : block)
		empty_status = _mm_or_si128(empty_status, elem);

	bool empty_block = _mm_movemask_epi8(_mm_cmpeq_epi32(empty_status, _mm_setzero_si128())) == 0xffff;
	if (empty_block) return PathFillBlockResult::Empty;

	for (unsigned int cnt = 0; cnt < MaskBlockSize; cnt++)
	{
		__m128i* input = &block[MaskBlockSize / 16 * cnt];
		__m128i* output = (__m128i*)(MaskBufferData + cnt * mask_texture_size);

		for (int sse_block = 0; sse_block < MaskBlockSize / 16; sse_block++)
			_mm_storeu_si128(&output[sse_block], input[sse_block]);
	}

	return PathFillBlockResult::Partial;
}

#else

PathFillBlockResult PathMaskBuffer::FillBlock(int xpos)
{
	if (IsFullBlock(xpos))
	{
		return PathFillBlockResult::Full;
	}

	memset(MaskBufferData, 0, MaskBufferSize);

	bool empty_block = true;
	for (unsigned int cnt = 0; cnt < ScanlineBlockSize; cnt++)
	{
		unsigned char* line = MaskBufferData + MaskBlockSize * (cnt / AntialiasLevel);
		while (Range[cnt].found)
		{
			int x0 = Range[cnt].x0;
			if (x0 >= xpos + ScanlineBlockSize)
				break;
			int x1 = Range[cnt].x1;

			x0 = std::max(x0, xpos);
			x1 = std::min(x1, xpos + ScanlineBlockSize);

			if (x0 >= x1)	// Done segment
			{
				Range[cnt].Next();
			}
			else
			{
				empty_block = false;
				for (int x = x0 - xpos; x < x1 - xpos; x++)
				{
					int pixel = line[x / AntialiasLevel];
					pixel = std::min(pixel + (256 / (AntialiasLevel * AntialiasLevel)), 255);
					line[x / AntialiasLevel] = pixel;
				}
				Range[cnt].x0 = x1;	// For next time
			}
		}
	}

	return empty_block ? PathFillBlockResult::Empty : PathFillBlockResult::Partial;
}

#endif

bool PathMaskBuffer::IsFullBlock(int xpos) const
{
	for (auto& elem : Range)
	{
		if (!elem.found)
		{
			return false;
		}
		if ((elem.x0 > xpos) || (elem.x1 < (xpos + ScanlineBlockSize - 1)))
		{
			return false;
		}
	}
	return true;
}
