#include <stdlib.h>
#include <string.h>

#include "dobject.h"
#include "m_alloc.h"

ClassInit::ClassInit (TypeInfo *type)
{
	type->RegisterType ();
}

TypeInfo **TypeInfo::m_Types;
unsigned short TypeInfo::m_NumTypes;
unsigned short TypeInfo::m_MaxTypes;

void TypeInfo::RegisterType ()
{
	if (m_NumTypes == m_MaxTypes)
	{
		m_MaxTypes = m_MaxTypes ? m_MaxTypes*2 : 32;
		m_Types = (TypeInfo **)Realloc (m_Types, m_MaxTypes * sizeof(*m_Types));
	}
	m_Types[m_NumTypes] = this;
	TypeIndex = m_NumTypes;
	m_NumTypes++;
}

const TypeInfo *TypeInfo::FindType (const char *name)
{
	unsigned short i;

	for (i = 0; i != m_NumTypes; i++)
		if (!strcmp (name, m_Types[i]->Name))
			return m_Types[i];

	return NULL;
}

TypeInfo DObject::_StaticType = { "DObject", NULL, sizeof(DObject) };
