#ifndef __LISTS_H__
#define __LISTS_H__

struct Node
{
	Node *Succ, *Pred;

	void Insert (Node *putAfter)
	{
		Succ = putAfter->Succ;
		Pred = putAfter;
		putAfter->Succ = this;
		Succ->Pred = this;
	}

	void InsertBefore (Node *putBefore)
	{
		Succ = putBefore;
		Pred = putBefore->Pred;
		putBefore->Pred = this;
		Pred->Succ = this;
	}

	void Remove ()
	{
		Pred->Succ = Succ;
		Succ->Pred = Pred;
	}
};

struct List
{
	Node *Head;
	const Node *const Tail;
	Node *TailPred;

	List () : Head ((Node *)&Tail), Tail (NULL), TailPred ((Node *)&Head)
	{
	}

	bool IsEmpty () const
	{
		return TailPred == (Node *)this;
	}

	void MakeEmpty ()
	{
		Head = (Node *)&Tail;
		TailPred = (Node *)&Head;
	}

	void AddHead (Node *node)
	{
		node->Succ = Head;
		node->Pred = (Node *)&Head;
		Head->Pred = node;
		Head = node;
	}

	void AddTail (Node *node)
	{
		node->Pred = TailPred;
		node->Succ = (Node *)&Tail;
		TailPred->Succ = node;
		TailPred = node;
	}
	
	Node *RemHead ()
	{
		Node *node = Head;
		if (node->Succ == NULL)
		{
			return NULL;
		}
		Head = node->Succ;
		Head->Pred = (Node *)&Head;
		return node;
	}

	Node *RemHeadQ ()	// Only use if list is definitely not empty
	{
		Node *node = Head;
		Head = node->Succ;
		Head->Pred = (Node *)&Head;
		return node;
	}

	Node *RemTail ()
	{
		Node *node = TailPred;
		if (node->Pred == NULL)
		{
			return NULL;
		}
		TailPred = node->Pred;
		TailPred->Succ = (Node *)&Tail;
		return node;
	}

	Node *RemTailQ ()	// Only use if list is definitely not empty
	{
		Node *node = TailPred;
		TailPred = node->Pred;
		TailPred->Succ = (Node *)&Tail;
		return node;
	}
};

#endif //__LISTS_H__
