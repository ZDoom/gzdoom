#pragma once

// System independent critical sections without polluting the namespace with the operating system headers.
class FInternalCriticalSection;
FInternalCriticalSection *CreateCriticalSection();
void DeleteCriticalSection(FInternalCriticalSection *c);
void EnterCriticalSection(FInternalCriticalSection *c);
void LeaveCriticalSection(FInternalCriticalSection *c);

// This is just a convenience wrapper around the function interface adjusted to use std::lock_guard
class FCriticalSection
{
public:
	FCriticalSection()
	{
		c = CreateCriticalSection();
	}
	
	~FCriticalSection()
	{
		DeleteCriticalSection(c);
	}

	void lock()
	{
		EnterCriticalSection(c);
	}
	
	void unlock()
	{
		LeaveCriticalSection(c);
	}

private:
	FInternalCriticalSection *c;

};
