#pragma once

#include <vector>
#include <cstdint>
#include <string>

struct SingleFontData
{
	std::vector<uint8_t> fontdata;
	std::string language;
};

std::vector<SingleFontData> LoadWidgetFontData(const std::string& name);
std::vector<uint8_t> LoadWidgetData(const std::string& name);
