#pragma once

struct FloatRect
{
	float left,top;
	float width,height;


	void Offset(float xofs,float yofs)
	{
		left+=xofs;
		top+=yofs;
	}
	void Scale(float xfac,float yfac)
	{
		left*=xfac;
		width*=xfac;
		top*=yfac;
		height*=yfac;
	}
};

struct DoubleRect
{
	double left, top;
	double width, height;


	void Offset(double xofs, double yofs)
	{
		left += xofs;
		top += yofs;
	}
	void Scale(double xfac, double yfac)
	{
		left *= xfac;
		width *= xfac;
		top *= yfac;
		height *= yfac;
	}
};
