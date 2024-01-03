#pragma once

class Point
{
public:
	Point() = default;
	Point(double x, double y) : x(x), y(y) { }

	double x = 0;
	double y = 0;

	Point& operator+=(const Point& p) { x += p.x; y += p.y; return *this; }
	Point& operator-=(const Point& p) { x -= p.x; y -= p.y; return *this; }
};

class Size
{
public:
	Size() = default;
	Size(double width, double height) : width(width), height(height) { }

	double width = 0;
	double height = 0;
};

class Rect
{
public:
	Rect() = default;
	Rect(const Point& p, const Size& s) : x(p.x), y(p.y), width(s.width), height(s.height) { }
	Rect(double x, double y, double width, double height) : x(x), y(y), width(width), height(height) { }

	Point pos() const { return { x, y }; }
	Size size() const { return { width, height }; }

	Point topLeft() const { return { x, y }; }
	Point topRight() const { return { x + width, y }; }
	Point bottomLeft() const { return { x, y + height }; }
	Point bottomRight() const { return { x + width, y + height }; }

	double left() const { return x; }
	double top() const { return y; }
	double right() const { return x + width; }
	double bottom() const { return y + height; }

	static Rect xywh(double x, double y, double width, double height) { return Rect(x, y, width, height); }
	static Rect ltrb(double left, double top, double right, double bottom) { return Rect(left, top, right - left, bottom - top); }

	static Rect shrink(Rect box, double left, double top, double right, double bottom)
	{
		box.x += left;
		box.y += top;
		box.width = std::max(box.width - left - right, 0.0);
		box.height = std::max(box.height - bottom - top, 0.0);
		return box;
	}

	bool contains(const Point& p) const { return (p.x >= x && p.x < x + width) && (p.y >= y && p.y < y + height); }

	double x = 0;
	double y = 0;
	double width = 0;
	double height = 0;
};

inline Point operator+(const Point& a, const Point& b) { return Point(a.x + b.x, a.y + b.y); }
inline Point operator-(const Point& a, const Point& b) { return Point(a.x - b.x, a.y - b.y); }
inline bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(const Point& a, const Point& b) { return a.x != b.x || a.y != b.y; }
inline bool operator==(const Size& a, const Size& b) { return a.width == b.width && a.height == b.height; }
inline bool operator!=(const Size& a, const Size& b) { return a.width != b.width || a.height != b.height; }
inline bool operator==(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }
inline bool operator!=(const Rect& a, const Rect& b) { return a.x != b.x || a.y != b.y || a.width != b.width || a.height != b.height; }

inline Point operator+(const Point& a, double b) { return Point(a.x + b, a.y + b); }
inline Point operator-(const Point& a, double b) { return Point(a.x - b, a.y - b); }
inline Point operator*(const Point& a, double b) { return Point(a.x * b, a.y * b); }
inline Point operator/(const Point& a, double b) { return Point(a.x / b, a.y / b); }

inline Size operator+(const Size& a, double b) { return Size(a.width + b, a.height + b); }
inline Size operator-(const Size& a, double b) { return Size(a.width - b, a.height - b); }
inline Size operator*(const Size& a, double b) { return Size(a.width * b, a.height * b); }
inline Size operator/(const Size& a, double b) { return Size(a.width / b, a.height / b); }
