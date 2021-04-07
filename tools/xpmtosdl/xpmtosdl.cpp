/* xpmtosdl.cpp
** Program to convert XPM files into C-compatible format suitable for direct inclusion.
**
**---------------------------------------------------------------------------
** Copyright 2021 Cacodemon345
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>

int ParseXPMArray(const char** zdoom_xpm, const char* fileout)
{
    int icon_width, icon_height, icon_colors, icon_chars_per_pixel;
    sscanf(zdoom_xpm[0], "%d %d %d %d", &icon_width, &icon_height, &icon_colors, &icon_chars_per_pixel);
    const char **colorlist = zdoom_xpm + 1;
    std::string pixmapcharacters, colorspec;
    std::map<std::string, uint32_t> chartocolormaps;
    for (int colorindex = 0; colorindex < icon_colors; colorindex++)
    {
        char colorstrbuffer[128];
        char colorspecbuffer[7] = {'0', '0', '0', '0', '0', '0', '\0'};
        memset(colorstrbuffer, 0, sizeof(colorstrbuffer));
        pixmapcharacters = std::string(colorlist[colorindex], icon_chars_per_pixel);
        // Non-color images are not supported.
        if (strchr(colorlist[colorindex] + icon_chars_per_pixel, 'c') == NULL)
            return -1;
        if (strchr(colorlist[colorindex], '#'))
            strcpy(colorstrbuffer, strchr(colorlist[colorindex], '#'));
        else
            sscanf(strchr(colorlist[colorindex] + icon_chars_per_pixel, 'c') + 1, "%s", colorstrbuffer);

        if (colorstrbuffer[0] == '#')
        {
            int sizeofspec = strlen(colorstrbuffer);
            switch (sizeofspec)
            {
            case 4: // RGB444 color
                colorspecbuffer[0] = colorspecbuffer[1] = colorstrbuffer[1];
                colorspecbuffer[2] = colorspecbuffer[3] = colorstrbuffer[2];
                colorspecbuffer[4] = colorspecbuffer[5] = colorstrbuffer[3];
                break;
            case 7: // RGB888 color
                memcpy(colorspecbuffer, colorstrbuffer + 1, 6);
                break;
            case 12: // Same as above but two whitespaces between each two hexadecimal characters.
                colorspecbuffer[0] = colorstrbuffer[1];
                colorspecbuffer[1] = colorstrbuffer[2];
                colorspecbuffer[2] = colorstrbuffer[5];
                colorspecbuffer[3] = colorstrbuffer[6];
                colorspecbuffer[4] = colorstrbuffer[9];
                colorspecbuffer[5] = colorstrbuffer[10];
                break;
            }
            chartocolormaps.emplace(pixmapcharacters, 0xFF000000 | (unsigned int)strtoul(colorspecbuffer, NULL, 16));
        }
        else
        {
            static const struct xpmX11colormaps
            {
                const char *known;
                uint32_t argb;
            } known[] =
            {
                    {"none", 0x00000000},
                    {"red", 0xFFFF0000},
                    {"green", 0xFF00FF00},
                    {"blue", 0xFF0000FF},
                    {"black", 0xFF000000},
                    {"white", 0xFFFFFFFF}
            };
            for (unsigned int i = 0; i < sizeof(known) / sizeof(known[0]); i++)
            {
                if (strncasecmp(known[i].known, colorstrbuffer, strlen(colorstrbuffer)) == 0)
                {
                    chartocolormaps.emplace(pixmapcharacters, known[i].argb);
                    break;
                }
            }
        }
    }
    const char **pixels = &colorlist[icon_colors];
    auto imagePixels = new uint32_t[icon_height * icon_width];
    for (int y = 0; y < icon_height; y++)
	{
		for (int x = 0; x < icon_width; x++)
		{
			auto color = chartocolormaps[std::string(&pixels[y][x * icon_chars_per_pixel],icon_chars_per_pixel)];
			if (color != 0)
			{
				imagePixels[y * icon_width + x] = color;
			}
		}
	}
    auto outputfile = fopen(fileout,"w");
    if (!outputfile)
    {
        printf("Could not open %s for writing: %s\n",outputfile, strerror(errno));
        return -1;
    }
    fprintf(outputfile,"#include <stdint.h>\n\n");
    fprintf(outputfile,"static const uint32_t icon_width = %d;\n", icon_width);
    fprintf(outputfile,"static const uint32_t icon_height = %d;\n\n", icon_height);
    fprintf(outputfile,"static const uint32_t icon_alpha_mask = 0x%08X;\n", 0xFF000000);
    fprintf(outputfile,"static const uint32_t icon_red_mask = 0x%08X;\n", 0xFF0000);
    fprintf(outputfile,"static const uint32_t icon_green_mask = 0x%08X;\n", 0xFF00);
    fprintf(outputfile,"static const uint32_t icon_blue_mask = 0x%08X;\n\n", 0xFF);
    fprintf(outputfile,"static const uint32_t iconpixeldata[icon_width * icon_height] = \n{\t\n\t");
    for (uint32_t i = 0; i < (icon_width * icon_height); i++)
    {
        fprintf(outputfile, "0x%08X%c", imagePixels[i], i == (icon_width * icon_height) - 1 ? ' ' : ',');
        if (((i + 1) % 4) == 0 && (i != (icon_width * icon_height) - 1)) fprintf(outputfile, "\n\t");
    }
    fprintf(outputfile,"\n};");
    fclose(outputfile);
    return 0;
}

int main(int argc, const char** argv)
{
    if (argc < 3)
    {
        printf("Less than two parameters specified.\n");
        return -1;
    }
    auto xpmfile = fopen(argv[1], "r");
    if (!xpmfile)
    {
        printf("Could not open %s: %s\n",argv[1], strerror(errno));
        return -1;
    }
    std::vector<std::string> lines;
    char* curline = nullptr;
    size_t sizeofline = 0;
    if (getline(&curline, &sizeofline, xpmfile) == -1)
    {
        printf("Premature end of file\n");
        free(curline);
        return -1;
    }
    // Strip newline of magic header.
    curline[strcspn(curline, "\r\n")] = 0;
    if (strncmp(curline, R"(/* XPM */)", sizeof(R"(/* XPM */)")) != 0)
    {
        printf("Invalid XPM magic header\n");
        free(curline);
        return -1;
    }
    while (getline(&curline, &sizeofline, xpmfile) != -1)
    {
        if (curline[0] != '"')
        {
            free(curline);
            curline = nullptr;
            continue;
        }
        std::string linestring = std::string(curline, sizeofline);
        linestring.resize(linestring.find_last_of('"'));
        linestring = linestring.substr(linestring.find_first_of('"') + 1);
        lines.push_back(linestring);
        free(curline);
        curline = nullptr;
    }
    std::vector<const char*> strToCharPtr;
    for (auto &curStr : lines)
    {
        strToCharPtr.push_back(curStr.c_str());
    }
    return ParseXPMArray(strToCharPtr.data(), argv[2]);
}