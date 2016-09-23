#ifndef __GL_CYCLER_H
#define __GL_CYCLER_H

class FSerializer;

enum CycleType
{
	CYCLE_Linear,
	CYCLE_Sin,
	CYCLE_Cos,
	CYCLE_SawTooth,
	CYCLE_Square
};

class FCycler;
FSerializer &Serialize(FSerializer &arc, const char *key, FCycler &c, FCycler *def);

class FCycler
{
	friend FSerializer &Serialize(FSerializer &arc, const char *key, FCycler &c, FCycler *def);

public:
   FCycler();
   void Update(float diff);
   void SetParams(float start, float end, float cycle);
   void ShouldCycle(bool sc) { m_shouldCycle = sc; }
   void SetCycleType(CycleType ct) { m_cycleType = ct; }
   float GetVal() { return m_current; }

   inline operator float () const { return m_current; }
   
protected:
   float m_start, m_end, m_current;
   float m_time, m_cycle;
   bool m_increment, m_shouldCycle;

   CycleType m_cycleType;
};


#endif
