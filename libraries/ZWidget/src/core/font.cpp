
#include "core/font.h"

class FontImpl : public Font
{
public:
	FontImpl(const std::string& name, double height) : Name(name), Height(height)
	{
	}

	const std::string& GetName() const override
	{
		return Name;
	}

	double GetHeight() const override
	{
		return Height;
	}

	std::string Name;
	double Height = 0.0;
};

std::shared_ptr<Font> Font::Create(const std::string& name, double height)
{
	return std::make_shared<FontImpl>(name, height);
}
