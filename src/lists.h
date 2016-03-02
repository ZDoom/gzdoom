/*
** lists.h
** Essentially, Amiga Exec lists and nodes
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

private:
	List &operator= (const List&) { return *this; }
};

#endif //__LISTS_H__
