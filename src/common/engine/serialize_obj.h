#pragma once

// These are in a separate header because they require some rather 'dirty' headers to work which should not be part of serializer.h

template<class T>
FSerializer &Serialize(FSerializer &arc, const char *key, TObjPtr<T> &value, TObjPtr<T> *)
{
	Serialize(arc, key, value.o, nullptr);
	return arc; 
}

template<class T>
FSerializer &Serialize(FSerializer &arc, const char *key, TObjPtr<T> &value, T *)
{
	Serialize(arc, key, value.o, nullptr);
	return arc;
}
