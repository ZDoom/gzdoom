#include "serializer.h"
#include "r_local.h"
#include "p_setup.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "a_sharedglobal.h"

//==========================================================================
//
//
//
//==========================================================================

void DThinker::SaveList(FSerializer &arc, DThinker *node)
{
	if (node != NULL)
	{
		while (!(node->ObjectFlags & OF_Sentinel))
		{
			assert(node->NextThinker != NULL && !(node->NextThinker->ObjectFlags & OF_EuthanizeMe));
			::Serialize<DThinker>(arc, nullptr, node, nullptr);
			node = node->NextThinker;
		}
	}
}

void DThinker::SerializeThinkers(FSerializer &arc, bool hubLoad)
{
	//DThinker *thinker;
	//BYTE stat;
	//int statcount;
	int i;

	if (arc.isWriting())
	{
		arc.BeginArray("thinkers");
		for (i = 0; i <= MAX_STATNUM; i++)
		{
			arc.BeginArray(nullptr);
			SaveList(arc, Thinkers[i].GetHead());
			SaveList(arc, FreshThinkers[i].GetHead());
			arc.EndArray();
		}
		arc.EndArray();
	}
	else
	{
	}
}



void DObject::SerializeUserVars(FSerializer &arc)
{
	PSymbolTable *symt;
	FName varname;
	//DWORD count, j;
	int *varloc = NULL;

	symt = &GetClass()->Symbols;

	if (arc.isWriting())
	{
		// Write all fields that aren't serialized by native code.
		//GetClass()->WriteValue(arc, this);
	}
	else 
	{
		//GetClass()->ReadValue(arc, this);
	}
}



void SerializeWorld(FSerializer &arc);

CCMD(writejson)
{
	DWORD t = I_MSTime();
	FSerializer arc;
	arc.OpenWriter();
	arc.BeginObject(nullptr);
	DThinker::SerializeThinkers(arc, false);
	SerializeWorld(arc);
	arc.WriteObjects();
	arc.EndObject();
	DWORD tt = I_MSTime();
	Printf("JSON generation took %d ms\n", tt - t);
	FILE *f = fopen("out.json", "wb");
	unsigned siz;
	const char *str = arc.GetOutput(&siz);
	fwrite(str, 1, siz, f);
	fclose(f);
	/*
	DWORD ttt = I_MSTime();
	Printf("JSON save took %d ms\n", ttt - tt);
	FDocument doc(arc.w->mOutString.GetString(), arc.w->mOutString.GetSize());
	DWORD tttt = I_MSTime();
	Printf("JSON parse took %d ms\n", tttt - ttt);
	*/
}
