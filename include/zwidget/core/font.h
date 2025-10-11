#pragma once

#include <memory>
#include <string>

class Font
{
public:
	virtual ~Font() = default;

	virtual const std::string& GetName() const = 0;
	virtual double GetHeight() const = 0;

	static std::shared_ptr<Font> Create(const std::string& name, double height);
};
