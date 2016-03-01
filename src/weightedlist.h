/*
** weightedlist.h
** A weighted list template class
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdlib.h>

#include "doomtype.h"

class FRandom;

template<class T>
class TWeightedList
{
	template<class U>
	struct Choice
	{
		Choice(WORD w, U v) : Next(NULL), Weight(w), RandomVal(0), Value(v) {}

		Choice<U> *Next;
		WORD Weight;
		BYTE RandomVal;	// 0 (never) - 255 (always)
		T Value;
	};

	public:
		TWeightedList (FRandom &pr) : Choices (NULL), RandomClass (pr) {}
		~TWeightedList ()
		{
			Choice<T> *choice = Choices;
			while (choice != NULL)
			{
				Choice<T> *next = choice->Next;
				delete choice;
				choice = next;
			}
		}

		void AddEntry (T value, WORD weight);
		T PickEntry () const;
		void ReplaceValues (T oldval, T newval);

	private:
		Choice<T> *Choices;
		FRandom &RandomClass;

		void RecalcRandomVals ();

		TWeightedList &operator= (const TWeightedList &) { return *this; }
};

template<class T> 
void TWeightedList<T>::AddEntry (T value, WORD weight)
{
	if (weight == 0)
	{ // If the weight is 0, don't bother adding it,
	  // since it will never be chosen.
		return;
	}

	Choice<T> **insAfter = &Choices, *insBefore = Choices;
	Choice<T> *theNewOne;

	while (insBefore != NULL && insBefore->Weight < weight)
	{
		insAfter = &insBefore->Next;
		insBefore = insBefore->Next;
	}
	theNewOne = new Choice<T> (weight, value);
	*insAfter = theNewOne;
	theNewOne->Next = insBefore;
	RecalcRandomVals ();
}

template<class T>
T TWeightedList<T>::PickEntry () const
{
	BYTE randomnum = RandomClass();
	Choice<T> *choice = Choices;

	while (choice != NULL && randomnum > choice->RandomVal)
	{
		choice = choice->Next;
	}
	return choice != NULL ? choice->Value : NULL;
}

template<class T>
void TWeightedList<T>::RecalcRandomVals ()
{
	// Redistribute the RandomVals so that they form the correct
	// distribution (as determined by the range of weights).

	int numChoices, weightSums;
	Choice<T> *choice;
	double randVal, weightDenom;

	if (Choices == NULL)
	{ // No choices, so nothing to do.
		return;
	}

	numChoices = 1;
	weightSums = 0;

	for (choice = Choices; choice->Next != NULL; choice = choice->Next)
	{
		++numChoices;
		weightSums += choice->Weight;
	}

	weightSums += choice->Weight;
	choice->RandomVal = 255;	// The last choice is always randomval 255

	randVal = 0.0;
	weightDenom = 1.0 / (double)weightSums;

	for (choice = Choices; choice->Next != NULL; choice = choice->Next)
	{
		randVal += (double)choice->Weight * weightDenom;
		choice->RandomVal = (BYTE)(randVal * 255.0);
	}
}

// Replace all values that match oldval with newval
template<class T>
void TWeightedList<T>::ReplaceValues(T oldval, T newval)
{
	Choice<T> *choice;

	for (choice = Choices; choice != NULL; choice = choice->Next)
	{
		if (choice->Value == oldval)
		{
			choice->Value = newval;
		}
	}
}
