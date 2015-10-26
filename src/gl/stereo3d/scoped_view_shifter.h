#ifndef GL_STEREO3D_SCOPED_VIEW_SHIFTER_H_
#define GL_STEREO3D_SCOPED_VIEW_SHIFTER_H_

#include "basictypes.h"

namespace s3d {

	/**
	 * Temporarily shift viewx, viewy, viewz
	 */
	class ScopedViewShifter
	{
	public:
		ScopedViewShifter(float dxyz[3]); // in meters
		~ScopedViewShifter();

	private:
		fixed_t cachedViewx;
		fixed_t cachedViewy;
		fixed_t cachedViewz;
	};

} /* namespace s3d */

#endif // GL_STEREO3D_SCOPED_VIEW_SHIFTER_H_
