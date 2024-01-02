#pragma once

#include <vector>
#include <cstdint>
#include <string>

struct SingleFontData
{
	std::vector<uint8_t> fontdata;
	std::vector<std::pair<uint32_t, uint32_t>> ranges;
	int language = -1;	// mainly useful if we start supporting Chinese so that we can use another font there than for Japanese.
};

std::vector<SingleFontData> LoadWidgetFontData(const std::string& name);
std::vector<uint8_t> LoadWidgetImageData(const std::string& name);
