#include "src/util/range.h"

namespace re2c
{

free_list<Range*> Range::vFreeList;

void Range::append_overlapping (Range * & head, Range * & tail, const Range * r)
{
	if (!head)
	{
		head = Range::ran (r->lb, r->ub);
		tail = head;
	}
	else if (tail->ub < r->lb)
	{
		tail->nx = Range::ran (r->lb, r->ub);
		tail = tail->nx;
	}
	else if (tail->ub < r->ub)
	{
		tail->ub = r->ub;
	}
}

Range * Range::add (const Range * r1, const Range * r2)
{
	Range * head = NULL;
	Range * tail = NULL;
	for (; r1 && r2;)
	{
		if (r1->lb < r2->lb)
		{
			append_overlapping (head, tail, r1);
			r1 = r1->nx;
		}
		else
		{
			append_overlapping (head, tail, r2);
			r2 = r2->nx;
		}
	}
	for (; r1; r1 = r1->nx)
	{
		append_overlapping (head, tail, r1);
	}
	for (; r2; r2 = r2->nx)
	{
		append_overlapping (head, tail, r2);
	}
	return head;
}

void Range::append (Range ** & ptail, uint32_t l, uint32_t u)
{
	Range * & tail = * ptail;
	tail = Range::ran (l, u);
	ptail = &tail->nx;
}

Range * Range::sub (const Range * r1, const Range * r2)
{
	Range * head = NULL;
	Range ** ptail = &head;
	while (r1)
	{
		if (!r2 || r2->lb >= r1->ub)
		{
			append (ptail, r1->lb, r1->ub);
			r1 = r1->nx;
		}
		else if (r2->ub <= r1->lb)
		{
			r2 = r2->nx;
		}
		else
		{
			if (r1->lb < r2->lb)
			{
				append (ptail, r1->lb, r2->lb);
			}
			while (r2 && r2->ub < r1->ub)
			{
				const uint32_t lb = r2->ub;
				r2 = r2->nx;
				const uint32_t ub = r2 && r2->lb < r1->ub
					? r2->lb
					: r1->ub;
				append (ptail, lb, ub);
			}
			r1 = r1->nx;
		}
	}
	return head;
}

} // namespace re2c
