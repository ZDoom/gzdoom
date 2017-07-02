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
		void Push(VisibleSprite *sprite);
		void Sort();

		TArray<VisibleSprite *> SortedSprites;

	private:
		TArray<VisibleSprite *> Sprites;
		TArray<unsigned int> StartIndices;
	};
}
