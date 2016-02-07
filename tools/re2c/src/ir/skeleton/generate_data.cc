#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/conf/msg.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/regexp/encoding/enc.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/path.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/util/u32lim.h"

namespace re2c
{

template <typename cunit_t, typename key_t>
	static Node::covers_t cover_one (FILE * input, FILE * keys, const path_t & path);

/*
 * note [generating skeleton path cover]
 *
 * With --skeleton switch we need to generate lots of data: strings that
 * correspond to various paths in DFA and match given regular expression.
 * We try to generate path cover (a set of paths that cover all skeleton
 * arcs at least once). Generation must stop as soon as the size of path
 * cover exceeds limit (in which case we'll only get a partial path cover).
 *
 * The algorithm walks graph nodes in deep-first order and assigns suffix
 * to each node (a path from this node to end node). In order to calculate
 * suffix for a given node the algorithm must know suffix for any child
 * node (end nodes are assigned empty suffix at start). Suffix is only
 * calculated once for each node and then reused as much times as the node
 * is visited. This is what reduces search space.
 *
 * The algorithm calculates prefix (multipath to current node). If current
 * node has already been assigned suffix, the algorithm immediately
 * calculates path cover from prefix and suffix. Otherwise it recurses to
 * child nodes (updating prefix on the go).
 *
 * The algorithm avoids eternal loops by maintaining loop counter for each
 * node. Loop counter is incremented on recursive enter and decremented on
 * recursive return. If loop counter is greater than 1, current branch is
 * abandoned and recursion returns immediately.
 *
 * See also note [counting skeleton edges].
 *
 */
template <typename cunit_t, typename key_t>
	void Node::cover (path_t & prefix, FILE * input, FILE * keys, covers_t &size)
{
	if (end () && suffix == NULL)
	{
		suffix = new path_t (rule, ctx);
	}
	if (suffix != NULL)
	{
		prefix.append (suffix);
		size = size + cover_one<cunit_t, key_t> (input, keys, prefix);
	}
	else if (loop < 2)
	{
		local_inc _ (loop);
		for (arcs_t::iterator i = arcs.begin ();
			i != arcs.end () && !size.overflow(); ++i)
		{
			path_t new_prefix = prefix;
			new_prefix.extend (i->first->rule, i->first->ctx, &i->second);
			i->first->cover<cunit_t, key_t> (new_prefix, input, keys, size);
			if (i->first->suffix != NULL && suffix == NULL)
			{
				suffix = new path_t (rule, ctx);
				suffix->extend (i->first->rule, i->first->ctx, &i->second);
				suffix->append (i->first->suffix);
			}
		}
	}
}

template <typename cunit_t, typename key_t>
	void Skeleton::generate_paths_cunit_key (FILE * input, FILE * keys)
{
	path_t prefix (nodes->rule, nodes->ctx);
	Node::covers_t size = Node::covers_t::from32(0u);

	nodes->cover<cunit_t, key_t> (prefix, input, keys, size);

	if (size.overflow ())
	{
		warning
			( NULL
			, line
			, false
			, "DFA %sis too large: can only generate partial path cover"
			, incond (cond).c_str ()
			);
	}
}

template <typename cunit_t>
	void Skeleton::generate_paths_cunit (FILE * input, FILE * keys)
{
	switch (sizeof_key)
	{
		case 4: generate_paths_cunit_key<cunit_t, uint32_t> (input, keys); break;
		case 2: generate_paths_cunit_key<cunit_t, uint16_t> (input, keys); break;
		case 1: generate_paths_cunit_key<cunit_t, uint8_t> (input, keys);  break;
	}
}

void Skeleton::generate_paths (FILE * input, FILE * keys)
{
	switch (opts->encoding.szCodeUnit ())
	{
		case 4: generate_paths_cunit<uint32_t> (input, keys); break;
		case 2: generate_paths_cunit<uint16_t> (input, keys); break;
		case 1: generate_paths_cunit<uint8_t>  (input, keys); break;
	}
}

void Skeleton::emit_data (const char * fname)
{
	const std::string input_name = std::string (fname) + "." + name + ".input";
	FILE * input = fopen (input_name.c_str (), "wb");
	if (!input)
	{
		error ("cannot open file: %s", input_name.c_str ());
		exit (1);
	}
	const std::string keys_name = std::string (fname) + "." + name + ".keys";
	FILE * keys = fopen (keys_name.c_str (), "wb");
	if (!keys)
	{
		error ("cannot open file: %s", keys_name.c_str ());
		exit (1);
	}

	generate_paths (input, keys);

	fclose (input);
	fclose (keys);
}

template <typename uintn_t> static uintn_t to_le(uintn_t n)
{
	uintn_t m;
	uint8_t *p = reinterpret_cast<uint8_t*>(&m);
	for (size_t i = 0; i < sizeof(uintn_t); ++i)
	{
		p[i] = static_cast<uint8_t>(n >> (i * 8));
	}
	return m;
}

template <typename key_t>
	static void keygen (FILE * f, size_t count, size_t len, size_t len_match, rule_rank_t match)
{
	const key_t m = Skeleton::rule2key<key_t> (match);

	const size_t keys_size = 3 * count;
	key_t * keys = new key_t [keys_size];
	for (uint32_t i = 0; i < keys_size;)
	{
		keys[i++] = to_le<key_t>(static_cast<key_t> (len));
		keys[i++] = to_le<key_t>(static_cast<key_t> (len_match));
		keys[i++] = to_le<key_t>(m);
	}
	fwrite (keys, sizeof (key_t), keys_size, f);
	delete [] keys;
}

template <typename cunit_t, typename key_t>
	static Node::covers_t cover_one (FILE * input, FILE * keys, const path_t & path)
{
	const size_t len = path.len ();

	size_t count = 0;
	for (size_t i = 0; i < len; ++i)
	{
		count = std::max (count, path[i]->size ());
	}

	const Node::covers_t size = Node::covers_t::from64(len) * Node::covers_t::from64(count);
	if (!size.overflow ())
	{
		// input
		const size_t buffer_size = size.uint32 ();
		cunit_t * buffer = new cunit_t [buffer_size];
		for (size_t i = 0; i < len; ++i)
		{
			const std::vector<uint32_t> & arc = *path[i];
			const size_t width = arc.size ();
			for (size_t j = 0; j < count; ++j)
			{
				const size_t k = j % width;
				buffer[j * len + i] = to_le<cunit_t>(static_cast<cunit_t> (arc[k]));
			}
		}
		fwrite (buffer, sizeof (cunit_t), buffer_size, input);
		delete [] buffer;

		// keys
		keygen<key_t> (keys, count, len, path.len_matching (), path.match ());
	}

	return size;
}

} // namespace re2c
