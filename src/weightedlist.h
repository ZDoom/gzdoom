#include <stdlib.h>

#include "doomtype.h"
#include "m_random.h"

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
		TWeightedList (pr_class_t pr) : Choices (NULL), RandomClass (pr) {}
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

	private:
		Choice<T> *Choices;
		const pr_class_t RandomClass;

		void RecalcRandomVals ();
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
	BYTE randomnum = P_Random (RandomClass);
	Choice<T> *choice = Choices;

	while (choice != NULL && randomnum > choice->RandomVal)
	{
		choice = choice->Next;
	}
	return choice->Value;
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
