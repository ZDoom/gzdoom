#include "src/ir/regexp/encoding/utf16/utf16_range.h"
#include "src/ir/regexp/encoding/range_suffix.h"

namespace re2c {

/*
 * Add word range [w1-w2].
 */
void UTF16addContinuous1(RangeSuffix * & root, uint32_t l, uint32_t h)
{
	RangeSuffix ** p = &root;
	for (;;)
	{
		if (*p == NULL)
		{
			*p = new RangeSuffix(l, h);
			break;
		}
		else if ((*p)->l == l && (*p)->h == h)
		{
			break;
		}
		else
			p = &(*p)->next;
	}
}

/*
 * Now that we have catenation of word ranges [l1-h1],[l2-h2],
 * we want to add it to existing range, merging suffixes on the fly.
 */
void UTF16addContinuous2(RangeSuffix * & root, uint32_t l_ld, uint32_t h_ld, uint32_t l_tr, uint32_t h_tr)
{
	RangeSuffix ** p = &root;
	for (;;)
	{
		if (*p == NULL)
		{
			*p = new RangeSuffix(l_tr, h_tr);
			p = &(*p)->child;
			break;
		}
		else if ((*p)->l == l_tr && (*p)->h == h_tr)
		{
			p = &(*p)->child;
			break;
		}
		else
			p = &(*p)->next;
	}
	for (;;)
	{
		if (*p == NULL)
		{
			*p = new RangeSuffix(l_ld, h_ld);
			break;
		}
		else if ((*p)->l == l_ld && (*p)->h == h_ld)
		{
			break;
		}
		else
			p = &(*p)->next;
	}
}

/*
 * Split range into sub-ranges that agree on leading surrogates.
 *
 * We have two Unicode runes, L and H, both map to UTF-16
 * surrogate pairs 'L1 L2' and 'H1 H2'.
 * We want to represent Unicode range [L - H] as a catenation
 * of word ranges [L1 - H1],[L2 - H2].
 *
 * This is only possible if the following condition holds:
 * if L1 /= H1, then L2 == 0xdc00 and H2 == 0xdfff.
 * This condition ensures that:
 * 	1) all possible UTF-16 sequences between L and H are allowed
 * 	2) no word ranges [w1 - w2] appear, such that w1 > w2
 *
 * E.g.:
 * [\U00010001-\U00010400] => [d800-d801],[dc01-dc00].
 * The last word range, [dc01-dc00], is incorrect: its lower bound
 * is greater than its upper bound. To fix this, we must split
 * the original range into two sub-ranges:
 * [\U00010001-\U000103ff] => [d800-d800],[dc01-dfff]
 * [\U00010400-\U00010400] => [d801-d801],[dc00-dc00]
 *
 * This function finds all such 'points of discontinuity'
 * and represents original range as alternation of continuous
 * sub-ranges.
 */
void UTF16splitByContinuity(RangeSuffix * & root, uint32_t l_ld, uint32_t h_ld, uint32_t l_tr, uint32_t h_tr)
{
	if (l_ld != h_ld)
	{
		if (l_tr > utf16::MIN_TRAIL_SURR)
		{
			UTF16splitByContinuity(root, l_ld, l_ld, l_tr, utf16::MAX_TRAIL_SURR);
			UTF16splitByContinuity(root, l_ld + 1, h_ld, utf16::MIN_TRAIL_SURR, h_tr);
			return;
		}
		if (h_tr < utf16::MAX_TRAIL_SURR)
		{
			UTF16splitByContinuity(root, l_ld, h_ld - 1, l_tr, utf16::MAX_TRAIL_SURR);
			UTF16splitByContinuity(root, h_ld, h_ld, utf16::MIN_TRAIL_SURR, h_tr);
			return;
		}
	}
	UTF16addContinuous2(root, l_ld, h_ld, l_tr, h_tr);
}

/*
 * Split range into sub-ranges, so that all runes in the same
 * sub-range have equal length of UTF-16 sequence. E.g., full
 * Unicode range [0-0x10FFFF] gets split into sub-ranges:
 * [0 - 0xFFFF]         (2-byte UTF-16 sequences)
 * [0x10000 - 0x10FFFF] (4-byte UTF-16 sequences)
 */
void UTF16splitByRuneLength(RangeSuffix * & root, utf16::rune l, utf16::rune h)
{
	if (l <= utf16::MAX_1WORD_RUNE)
	{
		if (h <= utf16::MAX_1WORD_RUNE)
		{
			UTF16addContinuous1(root, l, h);
		}
		else
		{
			UTF16addContinuous1(root, l, utf16::MAX_1WORD_RUNE);
			const uint32_t h_ld = utf16::lead_surr(h);
			const uint32_t h_tr = utf16::trail_surr(h);
			UTF16splitByContinuity(root, utf16::MIN_LEAD_SURR, h_ld, utf16::MIN_TRAIL_SURR, h_tr);
		}
	}
	else
	{
			const uint32_t l_ld = utf16::lead_surr(l);
			const uint32_t l_tr = utf16::trail_surr(l);
			const uint32_t h_ld = utf16::lead_surr(h);
			const uint32_t h_tr = utf16::trail_surr(h);
			UTF16splitByContinuity(root, l_ld, h_ld, l_tr, h_tr);
	}
}

} // namespace re2c
