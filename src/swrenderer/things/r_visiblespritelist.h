//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#pragma once

namespace swrenderer
{
	struct drawseg_t;
	struct vissprite_t;

	class VisibleSpriteList
	{
	public:
		static int MaxVisSprites;
		static vissprite_t **vissprites;
		static vissprite_t **firstvissprite;
		static vissprite_t **vissprite_p;

		static void Deinit();
		static void Clear();
		static vissprite_t *Add();

	private:
		static vissprite_t **lastvissprite;
	};

	class SortedVisibleSpriteList
	{
	public:
		static void Deinit();

		static void Sort(bool(*compare)(vissprite_t *, vissprite_t *), size_t first);

		static bool sv_compare(vissprite_t *a, vissprite_t *b);
		static bool sv_compare2d(vissprite_t *a, vissprite_t *b);

		static vissprite_t **spritesorter;
		static int vsprcount;

	private:
		static int spritesortersize;
	};
}
