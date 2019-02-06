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
		void Sort(RenderThread *thread);

		TArray<VisibleSprite *> SortedSprites;

	private:
		uint32_t FindSubsectorDepth(RenderThread *thread, const DVector2 &worldPos);
		uint32_t FindSubsectorDepth(RenderThread *thread, const DVector2 &worldPos, void *node);

		TArray<VisibleSprite *> Sprites;
		TArray<unsigned int> StartIndices;
	};
}
