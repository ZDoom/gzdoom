#ifndef GL_STEREO3D_H_
#define GL_STEREO3D_H_

#include <vector>
#include "gl/system/gl_system.h"


/* stereoscopic 3D API */
namespace s3d {


/* Subregion of current display window */
class Viewport
{
public:
	int x, y;
	int width, height;
};


/* Viewpoint of one eye */
class EyePose 
{
public:
	EyePose() {}
	virtual ~EyePose() {}
	virtual void GetProjection(float fov, float aspectRatio, float fovRatio, GLdouble outMatrix[4][4]) const;
	virtual Viewport GetViewport(const Viewport& fullViewport) const;
	virtual void GetViewShift(float yaw, float outViewShift[3]) const;
	virtual void SetUp() const {};
	virtual void TearDown() const {};
};


/* Base class for stereoscopic 3D rendering modes */
class Stereo3DMode
{
public:
	/* const_iterator cycles through the various eye viewpoints */
	typedef std::vector<const EyePose *>::const_iterator const_iterator;

	/* static methods for managing the selected stereoscopic view state */
	static const Stereo3DMode& getCurrentMode();

	Stereo3DMode();
	virtual ~Stereo3DMode();
	/* const_iterator cycles through the various eye viewpoints */
	virtual const_iterator begin() const { return eye_ptrs.begin(); }
	virtual const_iterator end() const { return eye_ptrs.end(); }
	/* hooks for setup and cleanup operations for each stereo mode */
	virtual void SetUp() const {};
	virtual void TearDown() const {};

protected:
	std::vector<const EyePose *> eye_ptrs;

private:
	static Stereo3DMode const * currentStereo3DMode;
	static void setCurrentMode(const Stereo3DMode& mode);
};


/**
*  Ordinary non-3D rendering
*/
class MonoView : public Stereo3DMode
{
public:
	static const MonoView& getInstance();

protected:
	MonoView() { eye_ptrs.push_back(&centralEye); }
	EyePose centralEye;
};


} /* namespace st3d */


#endif /* GL_STEREO3D_H_ */
