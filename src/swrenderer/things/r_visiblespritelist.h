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
	struct DrawSegment;
	class VisibleSprite;

	class VisibleSpriteList
	{
	public:
		void Clear();
		void PushPortal();
		void PopPortal();
		void Push(VisibleSprite *sprite, bool isVoxel = false);
		void Sort();

		TArray<VisibleSprite *> SortedSprites;

	private:
		TArray<VisibleSprite *> Sprites;
		TArray<unsigned int> StartIndices;
		bool DrewAVoxel = false;
	};
}
