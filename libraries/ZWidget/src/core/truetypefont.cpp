
// #define DUMP_GLYPH

#include "core/truetypefont.h"
#include "core/pathfill.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef DUMP_GLYPH
#include <fstream>
#endif

TrueTypeFont::TrueTypeFont(std::shared_ptr<TrueTypeFontFileData> initdata, int ttcFontIndex) : data(std::move(initdata))
{
	if (data->size() > 0x7fffffff)
		throw std::runtime_error("TTF file is larger than 2 gigabytes!");

	TrueTypeFileReader reader(data->data(), data->size());
	ttf_Tag versionTag = reader.ReadTag();
	reader.Seek(0);

	if (memcmp(versionTag.data(), "ttcf", 4) == 0) // TTC header
	{
		ttcHeader.Load(reader);
		if (ttcFontIndex >= ttcHeader.numFonts)
			throw std::runtime_error("TTC font index out of bounds");
		reader.Seek(ttcHeader.tableDirectoryOffsets[ttcFontIndex]);
	}

	directory.Load(reader);

	if (!directory.ContainsTTFOutlines())
		throw std::runtime_error("Only truetype outline fonts are supported");

	// Load required tables:

	reader = directory.GetReader(data->data(), data->size(), "head");
	head.Load(reader);

	reader = directory.GetReader(data->data(), data->size(), "hhea");
	hhea.Load(reader);

	reader = directory.GetReader(data->data(), data->size(), "maxp");
	maxp.Load(reader);

	reader = directory.GetReader(data->data(), data->size(), "hmtx");
	hmtx.Load(hhea, maxp, reader);

	reader = directory.GetReader(data->data(), data->size(), "name");
	name.Load(reader);

	reader = directory.GetReader(data->data(), data->size(), "OS/2");
	os2.Load(reader);

	reader = directory.GetReader(data->data(), data->size(), "cmap");
	cmap.Load(reader);

	LoadCharacterMapEncoding(reader);

	// Load TTF Outlines:

	reader = directory.GetReader(data->data(), data->size(), "loca");
	loca.Load(head, maxp, reader);

	glyf = directory.GetRecord("glyf");

#ifdef DUMP_GLYPH
	LoadGlyph(GetGlyphIndex('6'), 13.0);
#endif
}

TrueTypeTextMetrics TrueTypeFont::GetTextMetrics(double height) const
{
	double scale = height / head.unitsPerEm;

	TrueTypeTextMetrics metrics;
	metrics.ascender = os2.sTypoAscender * scale;
	metrics.descender = os2.sTypoDescender * scale;
	metrics.lineGap = os2.sTypoLineGap * scale;
	return metrics;
}

TrueTypeGlyph TrueTypeFont::LoadGlyph(uint32_t glyphIndex, double height) const
{
	double scale = height / head.unitsPerEm;
	double scaleX = 3.0f;
	double scaleY = -1.0f;

	ttf_uint16 advanceWidth = 0;
	ttf_int16 lsb = 0;
	if (glyphIndex >= hhea.numberOfHMetrics)
	{
		advanceWidth = hmtx.hMetrics[hhea.numberOfHMetrics - 1].advanceWidth;
		lsb = hmtx.leftSideBearings[glyphIndex - hhea.numberOfHMetrics];
	}
	else
	{
		advanceWidth = hmtx.hMetrics[glyphIndex].advanceWidth;
		lsb = hmtx.hMetrics[glyphIndex].lsb;
	}

	// Glyph is missing if the offset is the same as the next glyph (0 bytes glyph length)
	bool missing = glyphIndex + 1 < loca.offsets.size() ? loca.offsets[glyphIndex] == loca.offsets[glyphIndex + 1] : false;
	if (missing)
	{
		TrueTypeGlyph glyph;

		// TBD: gridfit or not?
		glyph.advanceWidth = (int)std::round(advanceWidth * scale * scaleX);
		glyph.leftSideBearing = (int)std::round(lsb * scale * scaleX);
		glyph.yOffset = 0;

		return glyph;
	}

	TTF_SimpleGlyph g;
	LoadGlyph(g, glyphIndex);

	// Create glyph path:
	PathFillDesc path;
	path.fill_mode = PathFillMode::winding;

	int startPoint = 0;
	int numberOfContours = g.endPtsOfContours.size();
	for (int i = 0; i < numberOfContours; i++)
	{
		int endPoint = g.endPtsOfContours[i];
		if (endPoint < startPoint)
			throw std::runtime_error("Invalid glyph");

		bool prevIsControlPoint;
		bool isControlPoint = !(g.flags[endPoint] & TTF_ON_CURVE_POINT);
		bool nextIsControlPoint = !(g.flags[startPoint] & TTF_ON_CURVE_POINT);
		if (isControlPoint)
		{
			Point nextpoint(g.points[startPoint].x, g.points[startPoint].y);
			if (nextIsControlPoint)
			{
				Point curpoint(g.points[endPoint].x, g.points[endPoint].y);
				Point midpoint = (curpoint + nextpoint) / 2;
				path.MoveTo(midpoint * scale);
				prevIsControlPoint = isControlPoint;
			}
			else
			{
				path.MoveTo(Point(g.points[startPoint].x, g.points[startPoint].y) * scale);
				prevIsControlPoint = false;
			}
		}
		else
		{
			path.MoveTo(Point(g.points[endPoint].x, g.points[endPoint].y) * scale);
			prevIsControlPoint = isControlPoint;
		}

		int pos = startPoint;
		while (pos <= endPoint)
		{
			int nextpos = pos + 1 <= endPoint ? pos + 1 : startPoint;
			bool isControlPoint = !(g.flags[pos] & TTF_ON_CURVE_POINT);
			bool nextIsControlPoint = !(g.flags[nextpos] & TTF_ON_CURVE_POINT);
			Point curpoint(g.points[pos].x, g.points[pos].y);
			if (isControlPoint)
			{
				Point nextpoint(g.points[nextpos].x, g.points[nextpos].y);
				if (nextIsControlPoint)
				{
					Point midpoint = (curpoint + nextpoint) / 2;
					path.BezierTo(curpoint * scale, midpoint * scale);
					prevIsControlPoint = isControlPoint;
					pos++;
				}
				else
				{
					path.BezierTo(curpoint * scale, nextpoint * scale);
					prevIsControlPoint = false;
					pos += 2;
				}
			}
			else
			{
				if (!prevIsControlPoint)
					path.LineTo(curpoint * scale);
				prevIsControlPoint = isControlPoint;
				pos++;
			}
		}
		path.Close();

		startPoint = endPoint + 1;
	}

	// Transform and find the final bounding box
	Point bboxMin, bboxMax;
	if (!path.subpaths.front().points.empty())
	{
		bboxMin = path.subpaths.front().points.front();
		bboxMax = path.subpaths.front().points.front();
		bboxMin.x *= scaleX;
		bboxMin.y *= scaleY;
		bboxMax.x *= scaleX;
		bboxMax.y *= scaleY;
		for (auto& subpath : path.subpaths)
		{
			for (auto& point : subpath.points)
			{
				point.x *= scaleX;
				point.y *= scaleY;
				bboxMin.x = std::min(bboxMin.x, point.x);
				bboxMin.y = std::min(bboxMin.y, point.y);
				bboxMax.x = std::max(bboxMax.x, point.x);
				bboxMax.y = std::max(bboxMax.y, point.y);
			}
		}
	}

	bboxMin.x = std::floor(bboxMin.x);
	bboxMin.y = std::floor(bboxMin.y);

	// Reposition glyph to bitmap so it begins at (0,0) for our bitmap
	for (auto& subpath : path.subpaths)
	{
		for (auto& point : subpath.points)
		{
			point.x -= bboxMin.x;
			point.y -= bboxMin.y;
		}
	}

#ifdef DUMP_GLYPH
	std::string svgxmlstart = R"(<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg width="1000px" height="1000px" viewBox="0 0 25 25" xmlns="http://www.w3.org/2000/svg" version="1.1">
<path fill-rule="evenodd" d=")";
	std::string svgxmlend = R"(" fill="red" />
</svg>)";

	std::ofstream out("c:\\development\\glyph.svg");
	out << svgxmlstart;

	for (auto& subpath : path.subpaths)
	{
		size_t pos = 0;
		out << "M" << subpath.points[pos].x << " " << subpath.points[pos].y << " ";
		pos++;
		for (PathFillCommand cmd : subpath.commands)
		{
			int count = 0;
			if (cmd == PathFillCommand::line)
			{
				out << "L";
				count = 1;
			}
			else if (cmd == PathFillCommand::quadradic)
			{
				out << "Q";
				count = 2;
			}
			else if (cmd == PathFillCommand::cubic)
			{
				out << "C";
				count = 3;
			}

			for (int i = 0; i < count; i++)
			{
				out << subpath.points[pos].x << " " << subpath.points[pos].y << " ";
				pos++;
			}
		}
		if (subpath.closed)
			out << "Z";
	}

	out << svgxmlend;
	out.close();
#endif

	TrueTypeGlyph glyph;

	// Rasterize the glyph
	glyph.width = (int)std::floor(bboxMax.x - bboxMin.x) + 1;
	glyph.height = (int)std::floor(bboxMax.y - bboxMin.y) + 1;
	glyph.grayscale.reset(new uint8_t[glyph.width * glyph.height]);
	uint8_t* grayscale = glyph.grayscale.get();
	path.Rasterize(grayscale, glyph.width, glyph.height);

	// TBD: gridfit or not?
	glyph.advanceWidth = (int)std::round(advanceWidth * scale * scaleX);
	glyph.leftSideBearing = (int)std::round(bboxMin.x);
	glyph.yOffset = (int)std::round(bboxMin.y);

	return glyph;
}

void TrueTypeFont::LoadGlyph(TTF_SimpleGlyph& g, uint32_t glyphIndex, int compositeDepth) const
{
	if (glyphIndex >= loca.offsets.size())
		throw std::runtime_error("Glyph index out of bounds");

	TrueTypeFileReader reader = glyf.GetReader(data->data(), data->size());
	reader.Seek(loca.offsets[glyphIndex]);

	ttf_int16 numberOfContours = reader.ReadInt16();
	ttf_int16 xMin = reader.ReadInt16();
	ttf_int16 yMin = reader.ReadInt16();
	ttf_int16 xMax = reader.ReadInt16();
	ttf_int16 yMax = reader.ReadInt16();

	if (numberOfContours > 0) // Simple glyph
	{
		int pointsOffset = g.points.size();
		for (ttf_uint16 i = 0; i < numberOfContours; i++)
			g.endPtsOfContours.push_back(pointsOffset + reader.ReadUInt16());

		ttf_uint16 instructionLength = reader.ReadUInt16();
		std::vector<ttf_uint8> instructions;
		instructions.resize(instructionLength);
		reader.Read(instructions.data(), instructions.size());

		int numPoints = (int)g.endPtsOfContours.back() - pointsOffset + 1;
		g.points.resize(pointsOffset + numPoints);

		while (g.flags.size() < g.points.size())
		{
			ttf_uint8 flag = reader.ReadUInt8();
			if (flag & TTF_REPEAT_FLAG)
			{
				ttf_uint8 repeatcount = reader.ReadUInt8();
				for (ttf_uint8 i = 0; i < repeatcount; i++)
					g.flags.push_back(flag);
			}
			g.flags.push_back(flag);
		}

		for (int i = pointsOffset; i < pointsOffset + numPoints; i++)
		{
			if (g.flags[i] & TTF_X_SHORT_VECTOR)
			{
				ttf_int16 x = reader.ReadUInt8();
				g.points[i].x = (g.flags[i] & TTF_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) ? x : -x;
			}
			else if (g.flags[i] & TTF_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)
			{
				g.points[i].x = 0;
			}
			else
			{
				g.points[i].x = reader.ReadInt16();
			}
		}

		for (int i = pointsOffset; i < pointsOffset + numPoints; i++)
		{
			if (g.flags[i] & TTF_Y_SHORT_VECTOR)
			{
				ttf_int16 y = reader.ReadUInt8();
				g.points[i].y = (g.flags[i] & TTF_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) ? y : -y;
			}
			else if (g.flags[i] & TTF_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR)
			{
				g.points[i].y = 0;
			}
			else
			{
				g.points[i].y = reader.ReadInt16();
			}
		}

		// Convert from relative coordinates to absolute
		for (int i = pointsOffset + 1; i < pointsOffset + numPoints; i++)
		{
			g.points[i].x += g.points[i - 1].x;
			g.points[i].y += g.points[i - 1].y;
		}
	}
	else if (numberOfContours < 0) // Composite glyph
	{
		if (compositeDepth == 8)
			throw std::runtime_error("Composite glyph recursion exceeded");

		int parentPointsOffset = g.points.size();

		bool weHaveInstructions = false;
		while (true)
		{
			ttf_uint16 flags = reader.ReadUInt16();
			ttf_uint16 childGlyphIndex = reader.ReadUInt16();

			int argument1, argument2;
			if (flags & TTF_ARG_1_AND_2_ARE_WORDS)
			{
				if (flags & TTF_ARGS_ARE_XY_VALUES)
				{
					argument1 = reader.ReadInt16();
					argument2 = reader.ReadInt16();
				}
				else
				{
					argument1 = reader.ReadUInt16();
					argument2 = reader.ReadUInt16();
				}
			}
			else
			{
				if (flags & TTF_ARGS_ARE_XY_VALUES)
				{
					argument1 = reader.ReadInt8();
					argument2 = reader.ReadInt8();
				}
				else
				{
					argument1 = reader.ReadUInt8();
					argument2 = reader.ReadUInt8();
				}
			}

			float mat2x2[4];
			bool transform = true;
			if (flags & TTF_WE_HAVE_A_SCALE)
			{
				ttf_F2DOT14 scale = F2DOT14_ToFloat(reader.ReadF2DOT14());
				mat2x2[0] = scale;
				mat2x2[1] = 0;
				mat2x2[2] = 0;
				mat2x2[3] = scale;
			}
			else if (flags & TTF_WE_HAVE_AN_X_AND_Y_SCALE)
			{
				mat2x2[0] = F2DOT14_ToFloat(reader.ReadF2DOT14());
				mat2x2[1] = 0;
				mat2x2[2] = 0;
				mat2x2[3] = F2DOT14_ToFloat(reader.ReadF2DOT14());
			}
			else if (flags & TTF_WE_HAVE_A_TWO_BY_TWO)
			{
				mat2x2[0] = F2DOT14_ToFloat(reader.ReadF2DOT14());
				mat2x2[1] = F2DOT14_ToFloat(reader.ReadF2DOT14());
				mat2x2[2] = F2DOT14_ToFloat(reader.ReadF2DOT14());
				mat2x2[3] = F2DOT14_ToFloat(reader.ReadF2DOT14());
			}
			else
			{
				transform = false;
			}

			int childPointsOffset = g.points.size();
			LoadGlyph(g, childGlyphIndex, compositeDepth + 1);

			if (transform)
			{
				for (int i = childPointsOffset; i < g.points.size(); i++)
				{
					float x = g.points[i].x * mat2x2[0] + g.points[i].y * mat2x2[1];
					float y = g.points[i].x * mat2x2[2] + g.points[i].y * mat2x2[3];
					g.points[i].x = x;
					g.points[i].y = y;
				}
			}

			float dx, dy;

			if (flags & TTF_ARGS_ARE_XY_VALUES)
			{
				dx = argument1;
				dy = argument2;

				// Spec states we must fall back to TTF_UNSCALED_COMPONENT_OFFSET if both flags are set
				if ((flags & (TTF_SCALED_COMPONENT_OFFSET | TTF_UNSCALED_COMPONENT_OFFSET)) == TTF_SCALED_COMPONENT_OFFSET)
				{
					float x = dx * mat2x2[0] + dy * mat2x2[1];
					float y = dx * mat2x2[2] + dy * mat2x2[3];
					dx = x;
					dy = y;
				}

				if (flags & TTF_ROUND_XY_TO_GRID)
				{
					// To do: round the offset to the pixel grid
				}
			}
			else
			{
				int parentPointIndex = parentPointsOffset + argument1;
				int childPointIndex = childPointsOffset + argument2;

				if ((size_t)parentPointIndex >= g.points.size() || (size_t)childPointIndex >= g.points.size())
					throw std::runtime_error("Invalid glyph offset");

				dx = g.points[parentPointIndex].x - g.points[childPointIndex].x;
				dy = g.points[parentPointIndex].y - g.points[childPointIndex].y;
			}

			for (int i = childPointsOffset; i < g.points.size(); i++)
			{
				g.points[i].x += dx;
				g.points[i].y += dy;
			}

			if (flags & TTF_USE_MY_METRICS)
			{
				// To do: this affects lsb + rsb calculations somehow
			}

			if (flags & TTF_WE_HAVE_INSTRUCTIONS)
			{
				weHaveInstructions = true;
			}

			if (!(flags & TTF_MORE_COMPONENTS))
				break;
		}

		if (weHaveInstructions)
		{
			ttf_uint16 instructionLength = reader.ReadUInt16();
			std::vector<ttf_uint8> instructions;
			instructions.resize(instructionLength);
			reader.Read(instructions.data(), instructions.size());
		}
	}
}

float TrueTypeFont::F2DOT14_ToFloat(ttf_F2DOT14 v)
{
	int a = ((ttf_int16)v) >> 14;
	int b = (v & 0x3fff);
	return a + b / (float)0x4000;
}

uint32_t TrueTypeFont::GetGlyphIndex(uint32_t c) const
{
	auto it = std::lower_bound(Ranges.begin(), Ranges.end(), c, [](const TTF_GlyphRange& range, uint32_t c) { return range.endCharCode < c; });
	if (it != Ranges.end() && c >= it->startCharCode && c <= it->endCharCode)
	{
		return it->startGlyphID + (c - it->startCharCode);
	}

	it = std::lower_bound(ManyToOneRanges.begin(), ManyToOneRanges.end(), c, [](const TTF_GlyphRange& range, uint32_t c) { return range.endCharCode < c; });
	if (it != ManyToOneRanges.end() && c >= it->startCharCode && c <= it->endCharCode)
	{
		return it->startGlyphID;
	}
	return 0;
}

void TrueTypeFont::LoadCharacterMapEncoding(TrueTypeFileReader& reader)
{
	// Look for the best encoding available that we support

	TTF_EncodingRecord record;
	if (!record.subtableOffset) record = cmap.GetEncoding(3, 12);
	if (!record.subtableOffset) record = cmap.GetEncoding(0, 4);
	if (!record.subtableOffset) record = cmap.GetEncoding(3, 1);
	if (!record.subtableOffset) record = cmap.GetEncoding(0, 3);
	if (!record.subtableOffset)
		throw std::runtime_error("No supported cmap encoding found in truetype file");

	reader.Seek(record.subtableOffset);

	ttf_uint16 format = reader.ReadUInt16();
	if (format == 4)
	{
		TTF_CMapSubtable4 subformat;
		subformat.Load(reader);

		for (uint16_t i = 0; i < subformat.segCount; i++)
		{
			ttf_uint16 startCode = subformat.startCode[i];
			ttf_uint16 endCode = subformat.endCode[i];
			ttf_uint16 idDelta = subformat.idDelta[i];
			ttf_uint16 idRangeOffset = subformat.idRangeOffsets[i];
			if (idRangeOffset == 0)
			{
				ttf_uint16 glyphId = startCode + idDelta; // Note: relies on modulo 65536

				TTF_GlyphRange range;
				range.startCharCode = startCode;
				range.endCharCode = endCode;
				range.startGlyphID = glyphId;
				Ranges.push_back(range);
			}
			else if (startCode <= endCode)
			{
				TTF_GlyphRange range;
				range.startCharCode = startCode;
				bool firstGlyph = true;
				for (ttf_uint16 c = startCode; c <= endCode; c++)
				{
					int offset = idRangeOffset / 2 + (c - startCode) - ((int)subformat.segCount - i);
					if (offset >= 0 && offset < subformat.glyphIdArray.size())
					{
						int glyphId = subformat.glyphIdArray[offset];
						if (firstGlyph)
						{
							range.startGlyphID = glyphId;
							firstGlyph = false;
						}
						else if (range.startGlyphID + (c - range.startCharCode) != glyphId)
						{
							range.endCharCode = c - 1;
							Ranges.push_back(range);
							range.startCharCode = c;
							range.startGlyphID = glyphId;
						}
					}
				}
				if (!firstGlyph)
				{
					range.endCharCode = endCode;
					Ranges.push_back(range);
				}
			}
		}
	}
	else if (format == 12 || format == 3)
	{
		TTF_CMapSubtable12 subformat;
		subformat.Load(reader);
		Ranges = std::move(subformat.groups);
	}
	else if (format == 13 || format == 3)
	{
		TTF_CMapSubtable13 subformat;
		subformat.Load(reader);
		ManyToOneRanges = std::move(subformat.groups);
	}
}

std::vector<TTCFontName> TrueTypeFont::GetFontNames(const std::shared_ptr<TrueTypeFontFileData>& data)
{
	if (data->size() > 0x7fffffff)
		throw std::runtime_error("TTF file is larger than 2 gigabytes!");

	TrueTypeFileReader reader(data->data(), data->size());
	ttf_Tag versionTag = reader.ReadTag();
	reader.Seek(0);

	std::vector<TTCFontName> names;
	if (memcmp(versionTag.data(), "ttcf", 4) == 0) // TTC header
	{
		TTC_Header ttcHeader;
		ttcHeader.Load(reader);

		for (size_t i = 0; i < ttcHeader.tableDirectoryOffsets.size(); i++)
		{
			reader.Seek(ttcHeader.tableDirectoryOffsets[i]);

			TTF_TableDirectory directory;
			directory.Load(reader);

			TTF_NamingTable name;
			auto name_reader = directory.GetReader(data->data(), data->size(), "name");
			name.Load(name_reader);

			names.push_back(name.GetFontName());
		}
	}
	else
	{
		TTF_TableDirectory directory;
		directory.Load(reader);

		TTF_NamingTable name;
		auto name_reader = directory.GetReader(data->data(), data->size(), "name");
		name.Load(name_reader);

		names.push_back(name.GetFontName());
	}
	return names;
}

/////////////////////////////////////////////////////////////////////////////

void TTF_CMapSubtable0::Load(TrueTypeFileReader& reader)
{
	length = reader.ReadUInt16();
	language = reader.ReadUInt16();
	glyphIdArray.resize(256);
	reader.Read(glyphIdArray.data(), glyphIdArray.size());
}

/////////////////////////////////////////////////////////////////////////////

void TTF_CMapSubtable4::Load(TrueTypeFileReader& reader)
{
	length = reader.ReadUInt16();
	language = reader.ReadUInt16();

	segCount = reader.ReadUInt16() / 2;
	ttf_uint16 searchRange = reader.ReadUInt16();
	ttf_uint16 entrySelector = reader.ReadUInt16();
	ttf_uint16 rangeShift = reader.ReadUInt16();

	endCode.reserve(segCount);
	startCode.reserve(segCount);
	idDelta.reserve(segCount);
	idRangeOffsets.reserve(segCount);
	for (ttf_uint16 i = 0; i < segCount; i++) endCode.push_back(reader.ReadUInt16());
	reservedPad = reader.ReadUInt16();
	for (ttf_uint16 i = 0; i < segCount; i++) startCode.push_back(reader.ReadUInt16());
	for (ttf_uint16 i = 0; i < segCount; i++) idDelta.push_back(reader.ReadInt16());
	for (ttf_uint16 i = 0; i < segCount; i++) idRangeOffsets.push_back(reader.ReadUInt16());

	int glyphIdArraySize = ((int)length - (8 + (int)segCount * 4) * sizeof(ttf_uint16)) / 2;
	if (glyphIdArraySize < 0)
		throw std::runtime_error("Invalid TTF cmap subtable 4 length");
	glyphIdArray.reserve(glyphIdArraySize);
	for (int i = 0; i < glyphIdArraySize; i++) glyphIdArray.push_back(reader.ReadUInt16());
}

/////////////////////////////////////////////////////////////////////////////

void TTF_CMapSubtable12::Load(TrueTypeFileReader& reader)
{
	reserved = reader.ReadUInt16();
	length = reader.ReadUInt32();
	language = reader.ReadUInt32();
	numGroups = reader.ReadUInt32();
	for (ttf_uint32 i = 0; i < numGroups; i++)
	{
		TTF_GlyphRange range;
		range.startCharCode = reader.ReadUInt32();
		range.endCharCode = reader.ReadUInt32();
		range.startGlyphID = reader.ReadUInt32();
		groups.push_back(range);
	}
}

/////////////////////////////////////////////////////////////////////////////

void TTF_TableRecord::Load(TrueTypeFileReader& reader)
{
	tableTag = reader.ReadTag();
	checksum = reader.ReadUInt32();
	offset = reader.ReadOffset32();
	length = reader.ReadUInt32();
}

/////////////////////////////////////////////////////////////////////////////

void TTF_TableDirectory::Load(TrueTypeFileReader& reader)
{
	sfntVersion = reader.ReadUInt32();
	numTables = reader.ReadUInt16();

	// opentype spec says we can't use these for security reasons, so we pretend they never was part of the header
	ttf_uint16 searchRange = reader.ReadUInt16();
	ttf_uint16 entrySelector = reader.ReadUInt16();
	ttf_uint16 rangeShift = reader.ReadUInt16();

	for (ttf_uint16 i = 0; i < numTables; i++)
	{
		TTF_TableRecord record;
		record.Load(reader);
		tableRecords.push_back(record);
	}
}

/////////////////////////////////////////////////////////////////////////////

void TTC_Header::Load(TrueTypeFileReader& reader)
{
	ttcTag = reader.ReadTag();
	majorVersion = reader.ReadUInt16();
	minorVersion = reader.ReadUInt16();

	if (!(majorVersion == 1 && minorVersion == 0) && !(majorVersion == 2 && minorVersion == 0))
		throw std::runtime_error("Unsupported TTC header version");

	numFonts = reader.ReadUInt32();
	for (ttf_uint16 i = 0; i < numFonts; i++)
	{
		tableDirectoryOffsets.push_back(reader.ReadOffset32());
	}

	if (majorVersion == 2 && minorVersion == 0)
	{
		dsigTag = reader.ReadUInt32();
		dsigLength = reader.ReadUInt32();
		dsigOffset = reader.ReadUInt32();
	}
}

/////////////////////////////////////////////////////////////////////////////

void TTF_CMap::Load(TrueTypeFileReader& reader)
{
	version = reader.ReadUInt16();
	numTables = reader.ReadUInt16();

	for (ttf_uint16 i = 0; i < numTables; i++)
	{
		TTF_EncodingRecord record;
		record.platformID = reader.ReadUInt16();
		record.encodingID = reader.ReadUInt16();
		record.subtableOffset = reader.ReadOffset32();
		encodingRecords.push_back(record);
	}
}

/////////////////////////////////////////////////////////////////////////////

void TTF_FontHeader::Load(TrueTypeFileReader& reader)
{
	majorVersion = reader.ReadUInt16();
	minorVersion = reader.ReadUInt16();
	fontRevision = reader.ReadFixed();
	checksumAdjustment = reader.ReadUInt32();
	magicNumber = reader.ReadUInt32();
	flags = reader.ReadUInt16();
	unitsPerEm = reader.ReadUInt16();
	created = reader.ReadLONGDATETIME();
	modified = reader.ReadLONGDATETIME();
	xMin = reader.ReadInt16();
	yMin = reader.ReadInt16();
	xMax = reader.ReadInt16();
	yMax = reader.ReadInt16();
	macStyle = reader.ReadUInt16();
	lowestRecPPEM = reader.ReadUInt16();
	fontDirectionHint = reader.ReadInt16();
	indexToLocFormat = reader.ReadInt16();
	glyphDataFormat = reader.ReadInt16();
}

/////////////////////////////////////////////////////////////////////////////

void TTF_HorizontalHeader::Load(TrueTypeFileReader& reader)
{
	majorVersion = reader.ReadUInt16();
	minorVersion = reader.ReadUInt16();
	ascender = reader.ReadFWORD();
	descender = reader.ReadFWORD();
	lineGap = reader.ReadFWORD();
	advanceWidthMax = reader.ReadUFWORD();
	minLeftSideBearing = reader.ReadFWORD();
	minRightSideBearing = reader.ReadFWORD();
	xMaxExtent = reader.ReadFWORD();
	caretSlopeRise = reader.ReadInt16();
	caretSlopeRun = reader.ReadInt16();
	caretOffset = reader.ReadInt16();
	reserved0 = reader.ReadInt16();
	reserved1 = reader.ReadInt16();
	reserved2 = reader.ReadInt16();
	reserved3 = reader.ReadInt16();
	metricDataFormat = reader.ReadInt16();
	numberOfHMetrics = reader.ReadUInt16();
}

/////////////////////////////////////////////////////////////////////////////

void TTF_HorizontalMetrics::Load(const TTF_HorizontalHeader& hhea, const TTF_MaximumProfile& maxp, TrueTypeFileReader& reader)
{
	for (ttf_uint16 i = 0; i < hhea.numberOfHMetrics; i++)
	{
		longHorMetric metric;
		metric.advanceWidth = reader.ReadUInt16();
		metric.lsb = reader.ReadInt16();
		hMetrics.push_back(metric);
	}

	int count = (int)maxp.numGlyphs - (int)hhea.numberOfHMetrics;
	if (count < 0)
		throw std::runtime_error("Invalid TTF file");

	for (int i = 0; i < count; i++)
	{
		leftSideBearings.push_back(reader.ReadInt16());
	}
}

/////////////////////////////////////////////////////////////////////////////

void TTF_MaximumProfile::Load(TrueTypeFileReader& reader)
{
	version = reader.ReadVersion16Dot16();
	numGlyphs = reader.ReadUInt16();

	if (version == (1 << 16)) // v1 only
	{
		maxPoints = reader.ReadUInt16();
		maxContours = reader.ReadUInt16();
		maxCompositePoints = reader.ReadUInt16();
		maxCompositeContours = reader.ReadUInt16();
		maxZones = reader.ReadUInt16();
		maxTwilightPoints = reader.ReadUInt16();
		maxStorage = reader.ReadUInt16();
		maxFunctionDefs = reader.ReadUInt16();
		maxInstructionDefs = reader.ReadUInt16();
		maxStackElements = reader.ReadUInt16();
		maxSizeOfInstructions = reader.ReadUInt16();
		maxComponentElements = reader.ReadUInt16();
		maxComponentDepth = reader.ReadUInt16();
	}
}

/////////////////////////////////////////////////////////////////////////////

static std::string unicode_to_utf8(unsigned int value)
{
	char text[8];

	if ((value < 0x80) && (value > 0))
	{
		text[0] = (char)value;
		text[1] = 0;
	}
	else if (value < 0x800)
	{
		text[0] = (char)(0xc0 | (value >> 6));
		text[1] = (char)(0x80 | (value & 0x3f));
		text[2] = 0;
	}
	else if (value < 0x10000)
	{
		text[0] = (char)(0xe0 | (value >> 12));
		text[1] = (char)(0x80 | ((value >> 6) & 0x3f));
		text[2] = (char)(0x80 | (value & 0x3f));
		text[3] = 0;
	}
	else if (value < 0x200000)
	{
		text[0] = (char)(0xf0 | (value >> 18));
		text[1] = (char)(0x80 | ((value >> 12) & 0x3f));
		text[2] = (char)(0x80 | ((value >> 6) & 0x3f));
		text[3] = (char)(0x80 | (value & 0x3f));
		text[4] = 0;
	}
	else if (value < 0x4000000)
	{
		text[0] = (char)(0xf8 | (value >> 24));
		text[1] = (char)(0x80 | ((value >> 18) & 0x3f));
		text[2] = (char)(0x80 | ((value >> 12) & 0x3f));
		text[3] = (char)(0x80 | ((value >> 6) & 0x3f));
		text[4] = (char)(0x80 | (value & 0x3f));
		text[5] = 0;
	}
	else if (value < 0x80000000)
	{
		text[0] = (char)(0xfc | (value >> 30));
		text[1] = (char)(0x80 | ((value >> 24) & 0x3f));
		text[2] = (char)(0x80 | ((value >> 18) & 0x3f));
		text[3] = (char)(0x80 | ((value >> 12) & 0x3f));
		text[4] = (char)(0x80 | ((value >> 6) & 0x3f));
		text[5] = (char)(0x80 | (value & 0x3f));
		text[6] = 0;
	}
	else
	{
		text[0] = 0;	// Invalid wchar value
	}
	return text;
}

void TTF_NamingTable::Load(TrueTypeFileReader& reader)
{
	version = reader.ReadUInt16();
	count = reader.ReadUInt16();
	storageOffset = reader.ReadOffset16();
	for (ttf_uint16 i = 0; i < count; i++)
	{
		NameRecord record;
		record.platformID = reader.ReadUInt16();
		record.encodingID = reader.ReadUInt16();
		record.languageID = reader.ReadUInt16();
		record.nameID = reader.ReadUInt16();
		record.length = reader.ReadUInt16();
		record.stringOffset = reader.ReadOffset16();
		nameRecord.push_back(record);
	}

	if (version == 1)
	{
		langTagCount = reader.ReadUInt16();
		for (ttf_uint16 i = 0; i < langTagCount; i++)
		{
			LangTagRecord record;
			ttf_uint16 length;
			ttf_Offset16 langTagOffset;
			langTagRecord.push_back(record);
		}
	}

	for (NameRecord& record : nameRecord)
	{
		if (record.length > 0 && record.platformID == 3 && record.encodingID == 1)
		{
			reader.Seek(storageOffset + record.stringOffset);
			ttf_uint16 ucs2len = record.length / 2;
			for (ttf_uint16 i = 0; i < ucs2len; i++)
			{
				record.text += unicode_to_utf8(reader.ReadUInt16());
			}
		}
	}
}

TTCFontName TTF_NamingTable::GetFontName() const
{
	TTCFontName fname;
	for (const auto& record : nameRecord)
	{
		if (record.nameID == 1) fname.FamilyName = record.text;
		if (record.nameID == 2) fname.SubfamilyName = record.text;
		if (record.nameID == 3) fname.UniqueID = record.text;
		if (record.nameID == 4) fname.FullName = record.text;
		if (record.nameID == 5) fname.VersionString = record.text;
		if (record.nameID == 6) fname.PostscriptName = record.text;
	}
	return fname;
}

/////////////////////////////////////////////////////////////////////////////

void TTF_OS2Windows::Load(TrueTypeFileReader& reader)
{
	version = reader.ReadUInt16();
	xAvgCharWidth = reader.ReadInt16();
	usWeightClass = reader.ReadUInt16();
	usWidthClass = reader.ReadUInt16();
	fsType = reader.ReadUInt16();
	ySubscriptXSize = reader.ReadInt16();
	ySubscriptYSize = reader.ReadInt16();
	ySubscriptXOffset = reader.ReadInt16();
	ySubscriptYOffset = reader.ReadInt16();
	ySuperscriptXSize = reader.ReadInt16();
	ySuperscriptYSize = reader.ReadInt16();
	ySuperscriptXOffset = reader.ReadInt16();
	ySuperscriptYOffset = reader.ReadInt16();
	yStrikeoutSize = reader.ReadInt16();
	yStrikeoutPosition = reader.ReadInt16();
	sFamilyClass = reader.ReadInt16();
	for (int i = 0; i < 10; i++)
	{
		panose[i] = reader.ReadUInt8();
	}
	ulUnicodeRange1 = reader.ReadUInt32();
	ulUnicodeRange2 = reader.ReadUInt32();
	ulUnicodeRange3 = reader.ReadUInt32();
	ulUnicodeRange4 = reader.ReadUInt32();
	achVendID = reader.ReadTag();
	fsSelection = reader.ReadUInt16();
	usFirstCharIndex = reader.ReadUInt16();
	usLastCharIndex = reader.ReadUInt16();
	sTypoAscender = reader.ReadInt16();
	sTypoDescender = reader.ReadInt16();
	sTypoLineGap = reader.ReadInt16();
	if (!reader.IsEndOfData()) // may be missing in v0 due to a bug in Apple's TTF documentation
	{
		usWinAscent = reader.ReadUInt16();
		usWinDescent = reader.ReadUInt16();
	}
	if (version >= 1)
	{
		ulCodePageRange1 = reader.ReadUInt32();
		ulCodePageRange2 = reader.ReadUInt32();
	}
	if (version >= 2)
	{
		sxHeight = reader.ReadInt16();
		sCapHeight = reader.ReadInt16();
		usDefaultChar = reader.ReadUInt16();
		usBreakChar = reader.ReadUInt16();
		usMaxContext = reader.ReadUInt16();
	}
	if (version >= 5)
	{
		usLowerOpticalPointSize = reader.ReadUInt16();
		usUpperOpticalPointSize = reader.ReadUInt16();
	}
}

/////////////////////////////////////////////////////////////////////////////

void TTF_IndexToLocation::Load(const TTF_FontHeader& head, const TTF_MaximumProfile& maxp, TrueTypeFileReader& reader)
{
	int count = (int)maxp.numGlyphs + 1;
	if (head.indexToLocFormat == 0)
	{
		offsets.reserve(count);
		for (int i = 0; i < count; i++)
		{
			offsets.push_back((ttf_Offset32)reader.ReadOffset16() * 2);
		}
	}
	else
	{
		offsets.reserve(count);
		for (int i = 0; i < count; i++)
		{
			offsets.push_back(reader.ReadOffset32());
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

ttf_uint8 TrueTypeFileReader::ReadUInt8()
{
	ttf_uint8 v; Read(&v, 1); return v;
}

ttf_uint16 TrueTypeFileReader::ReadUInt16()
{
	ttf_uint8 v[2]; Read(v, 2); return (((ttf_uint16)v[0]) << 8) | (ttf_uint16)v[1];
}

ttf_uint24 TrueTypeFileReader::ReadUInt24()
{
	ttf_uint8 v[3]; Read(v, 3); return (((ttf_uint32)v[0]) << 16) | (((ttf_uint32)v[1]) << 8) | (ttf_uint32)v[2];
}

ttf_uint32 TrueTypeFileReader::ReadUInt32()
{
	ttf_uint8 v[4]; Read(v, 4); return (((ttf_uint32)v[0]) << 24) | (((ttf_uint32)v[1]) << 16) | (((ttf_uint32)v[2]) << 8) | (ttf_uint32)v[3];
}

ttf_int8 TrueTypeFileReader::ReadInt8()
{
	return ReadUInt8();
}

ttf_int16 TrueTypeFileReader::ReadInt16()
{
	return ReadUInt16();
}

ttf_int32 TrueTypeFileReader::ReadInt32()
{
	return ReadUInt32();
}

ttf_Fixed TrueTypeFileReader::ReadFixed()
{
	return ReadUInt32();
}

ttf_UFWORD TrueTypeFileReader::ReadUFWORD()
{
	return ReadUInt16();
}

ttf_FWORD TrueTypeFileReader::ReadFWORD()
{
	return ReadUInt16();
}

ttf_F2DOT14 TrueTypeFileReader::ReadF2DOT14()
{
	return ReadUInt16();
}

ttf_LONGDATETIME TrueTypeFileReader::ReadLONGDATETIME()
{
	ttf_uint8 v[8]; Read(v, 8);
	return
		(((ttf_LONGDATETIME)v[0]) << 56) | (((ttf_LONGDATETIME)v[1]) << 48) | (((ttf_LONGDATETIME)v[2]) << 40) | (((ttf_LONGDATETIME)v[3]) << 32) |
		(((ttf_LONGDATETIME)v[4]) << 24) | (((ttf_LONGDATETIME)v[5]) << 16) | (((ttf_LONGDATETIME)v[6]) << 8) | (ttf_LONGDATETIME)v[7];
}

ttf_Tag TrueTypeFileReader::ReadTag()
{
	ttf_Tag v; Read(v.data(), v.size()); return v;
}

ttf_Offset16 TrueTypeFileReader::ReadOffset16()
{
	return ReadUInt16();
}

ttf_Offset24 TrueTypeFileReader::ReadOffset24()
{
	return ReadUInt24();
}

ttf_Offset32 TrueTypeFileReader::ReadOffset32()
{
	return ReadUInt32();
}

ttf_Version16Dot16 TrueTypeFileReader::ReadVersion16Dot16()
{
	return ReadUInt32();
}

void TrueTypeFileReader::Seek(size_t newpos)
{
	if (newpos > size)
		throw std::runtime_error("Invalid TTF file");

	pos = newpos;
}

void TrueTypeFileReader::Read(void* output, size_t count)
{
	if (pos + count > size)
		throw std::runtime_error("Unexpected end of TTF file");
	memcpy(output, data + pos, count);
	pos += count;
}
