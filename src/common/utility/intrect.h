#pragma once


struct IntRect
{
	int left, top;
	int width, height;

	void Offset(int xofs, int yofs)
	{
		left += xofs;
		top += yofs;
	}

	void AddToRect(int x, int y)
	{
		if (x < left)
			left = x;
		if (x > left + width)
			width = x - left;

		if (y < top)
			top = y;
		if (y > top + height)
			height = y - top;
	}

	int Left() const
	{
		return left;
	}

	int Top() const
	{
		return top;
	}


	int Right() const
	{
		return left + width;
	}

	int Bottom() const
	{
		return top + height;
	}


};

