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
   void Update(double diff);
   void SetParams(double start, double end, double cycle, bool update = false);
   void ShouldCycle(bool sc) { m_shouldCycle = sc; }
   void SetCycleType(CycleType ct) { m_cycleType = ct; }
   double GetVal() { return m_current; }

   inline operator double () const { return m_current; }
   
protected:
   double m_start, m_end, m_current;
   double m_time, m_cycle;
   bool m_increment, m_shouldCycle;

   CycleType m_cycleType;
};


#endif
