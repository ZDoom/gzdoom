

#include <math.h>
#include <float.h>
#include "gl/utility/gl_geometric.h"

static Vector axis[3] = 
{
   Vector(1.0f, 0.0f, 0.0f),
   Vector(0.0f, 1.0f, 0.0f),
   Vector(0.0f, 0.0f, 1.0f)
};



Vector Vector::Cross(Vector &v)
{
   float x, y, z;
   Vector cp;

   x = Y() * v.Z() - Z() * v.Y();
   y = Z() * v.X() - X() * v.Z();
   z = X() * v.Y() - Y() * v.X();

   cp.Set(x, y, z);

   return cp;
}


Vector Vector::operator- (Vector &v)
{
   float x, y, z;
   Vector vec;

   x = X() - v.X();
   y = Y() - v.Y();
   z = Z() - v.Z();

   vec.Set(x, y, z);

   return vec;
}


Vector Vector::operator+ (Vector &v)
{
   float x, y, z;
   Vector vec;

   x = X() + v.X();
   y = Y() + v.Y();
   z = Z() + v.Z();

   vec.Set(x, y, z);

   return vec;
}


Vector Vector::operator* (float f)
{
   Vector vec(X(), Y(), Z());

   vec.Scale(f);

   return vec;
}


Vector Vector::operator/ (float f)
{
   Vector vec(X(), Y(), Z());

   vec.Scale(1.f / f);

   return vec;
}


bool Vector::operator== (Vector &v)
{
   return X() == v.X() && Y() == v.Y() && Z() == v.Z();
}


void Vector::GetRightUp(Vector &right, Vector &up)
{
   Vector n(X(), Y(), Z());
   Vector fn(fabsf(n.X()), fabsf(n.Y()), fabsf(n.Z()));
   int major = 0;

   if (fn[1] > fn[major]) major = 1;
   if (fn[2] > fn[major]) major = 2;

   // build right vector by hand
   if (fabsf(fn[0]-1.0f) < FLT_EPSILON || fabsf(fn[1]-1.0f) < FLT_EPSILON || fabsf(fn[2]-1.0f) < FLT_EPSILON)
   {
      if (major == 0 && n[0] > 0.f)
      {
         right.Set(0.f, 0.f, -1.f);
      }
      else if (major == 0)
      {
         right.Set(0.f, 0.f, 1.f);
      }
      
      if (major == 1 || (major == 2 && n[2] > 0.f))
      {
         right.Set(1.f, 0.f, 0.f);
      }
   
      if (major == 2 && n[2] < 0.0f)
      {
         right.Set(-1.f, 0.f, 0.f);
      }
   }
   else
   {
      right = axis[major].Cross(n);
   }
  
   up = n.Cross(right);
   right.Normalize();
   up.Normalize();
}


void Vector::Scale(float scale)
{
   float x, y, z;

   x = X() * scale;
   y = Y() * scale;
   z = Z() * scale;

   Set(x, y, z);
}


Vector Vector::ProjectVector(Vector &a)
{
   Vector res, b;

   b.Set(X(), Y(), Z());
   res.Set(a.X(), a.Y(), a.Z());

   res.Scale(a.Dot(b) / a.Dot(a));

   return res;
}


Vector Vector::ProjectPlane(Vector &right, Vector &up)
{
   Vector src(X(), Y(), Z());
   Vector t1, t2;

   t1 = src.ProjectVector(right);
   t2 = src.ProjectVector(up);

   return t1 + t2;
}






void Plane::Init(float *v1, float *v2, float *v3)
{
   Vector vec1, vec2, vec3;

   vec1.Set(v1);
   vec2.Set(v2);
   vec3.Set(v3);

#ifdef _MSC_VER
   m_normal = (vec2 - vec1).Cross(vec3 - vec1);
#else
   Vector tmpVec = vec3 - vec1;
   m_normal = (vec2 - vec1).Cross(tmpVec);
#endif
   m_normal.Normalize();
   m_d = vec3.Dot(m_normal) * -1.f;
}


#define FNOTEQUAL(a, b) (fabsf(a - b) > 0.001f)
void Plane::Init(float *verts, int numVerts)
{
   float *v[3], *t;
   int i, curVert;

   if (numVerts < 3) return;

   curVert = 1;
   v[0] = verts + 0;
   for (i = 1; i < numVerts; i++)
   {
      t = verts + (i * 3);
      if (FNOTEQUAL(t[0], v[curVert - 1][0]) || FNOTEQUAL(t[1], v[curVert - 1][1]) || FNOTEQUAL(t[2], v[curVert - 1][2]))
      {
         v[curVert] = t;
         curVert++;
      }
      if (curVert == 3) break;
   }

   if (curVert != 3)
   {
      // degenerate triangle, no valid normal
      return;
   }

   Init(v[0], v[1], v[2]);
}


void Plane::Init(float a, float b, float c, float d)
{
   m_normal.Set(a, b, c);
   m_d = d / m_normal.Length();
   m_normal.Normalize();
}


void Plane::Set(secplane_t &plane)
{
   m_normal.Set((float)plane.Normal().X, (float)plane.Normal().Z, (float)plane.Normal().Y);
   //m_normal.Normalize(); the vector is already normalized
   m_d = (float)plane.fD();
}


float Plane::DistToPoint(float x, float y, float z)
{
   Vector p;

   p.Set(x, y, z);

   return m_normal.Dot(p) + m_d;
}


bool Plane::PointOnSide(float x, float y, float z)
{
   return DistToPoint(x, y, z) < 0.f;
}


