/**************************************************************************************************
"POLYMOST" code written by Ken Silverman
**************************************************************************************************/

#include "c_cvars.h"

typedef void (*pmostcallbacktype)(double *dpx, double *dpy, int n, void *userdata);

class PolyClipper
{
public:
	PolyClipper();
	~PolyClipper();

	void InitMosts (double *px, double *py, int n);
	bool TestVisibleMost (float x0, float x1);
	int DoMost (float x0, float y0, float x1, float y1, pmostcallbacktype callback, void *callbackdata);

private:
    struct vsptype
	{
		float X, Cy[2], Fy[2];
		int Tag, CTag, FTag;
		vsptype *Next, *Prev;
	};

	enum { GROUP_SIZE = 128 };

	struct vspgroup
	{
		vspgroup (vsptype *sentinel);

		vspgroup *NextGroup;
		vsptype vsp[GROUP_SIZE];
	};

	vsptype EmptyList;
	vsptype UsedList;
	vspgroup vsps;
	vsptype Solid;
    
	int GTag;

	vsptype *vsinsaft (vsptype *vsp);
	void EmptyAll ();
	void AddGroup ();
	vsptype *GetVsp ();
	void FreeVsp (vsptype *vsp);

	friend void drawpolymosttest();

};

EXTERN_CVAR(Bool, testpolymost)

extern void drawpolymosttest();
struct event_t; void Polymost_Responder (event_t *ev);
