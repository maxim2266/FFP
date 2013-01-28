/*
Copyright (c) 2013, Maxim Konakov
 All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list 
   of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list 
   of conditions and the following disclaimer in the documentation and/or other materials 
   provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS 
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER 
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "fix_parser_impl.h"

#include <malloc.h>
#include <stdlib.h>
#include <memory.h>

// FIX message node -------------------------------------------------------------------------------
static const size_t caps[] = { 0u, 23u, 47u, 101u, 199u, 401u, 809u };

struct fix_group_node* alloc_group_node()
{
	return ALLOC_Z(struct fix_group_node);
}

static void free_group_node(struct fix_group_node* pnode);

static
void clear_groups(struct fix_group_node* pnode)
{
	size_t i;

	for(i = 0; i < caps[pnode->cap_index]; ++i)
	{
		struct fix_group_node* p = pnode->buff[i].group;

		while(p)
		{
			struct fix_group_node* const next = p->next;

			free_group_node(p);
			p = next;
		}
	}
}

void clear_group_node(struct fix_group_node* pnode)
{
	if(pnode && pnode->buff)
	{
		clear_groups(pnode);
		FREE(pnode->buff);
	}
}

static
void free_group_node(struct fix_group_node* pnode)
{
	clear_group_node(pnode);
	FREE(pnode);
}

void set_group_node_empty(struct fix_group_node* pnode)
{
	if(pnode->buff && pnode->size > 0)
	{
		clear_groups(pnode);
		memset(pnode->buff, 0, sizeof(struct fix_tag) * caps[pnode->cap_index]);
		pnode->size = 0;
	}
}

static
struct fix_tag* find_fix_tag(const struct fix_group_node* pnode, size_t tag)
{
	struct fix_tag* p = NULL;
	const size_t tbl_size = caps[pnode->cap_index];

	if(tbl_size > 0)
	{
		const size_t h2 = 1 + (tag % (tbl_size - 1));
		size_t h1 = 2654435769u * tag;

		p = &pnode->buff[h1 % tbl_size];

		// find the tag or an empty slot
		while(p->tag > 0 && p->tag != tag)
		{
			h1 += h2;
			p = &pnode->buff[h1 % tbl_size];
		}
	}

	return p;
}

static
boolean expand_message_node(struct fix_group_node* pnode)
{
	if(pnode->cap_index > 0)
	{
		size_t i;
		struct fix_tag* old_buff;

		if(pnode->cap_index == _countof(caps) - 1)
			return NO;	// too many tags

		old_buff = pnode->buff;
		pnode->buff = ALLOC_NZ(caps[++pnode->cap_index], struct fix_tag);

		for(i = 0; i < caps[pnode->cap_index]; ++i)
		{
			struct fix_tag* const p = &old_buff[i];

			if(p->tag > 0)
				*find_fix_tag(pnode, p->tag) = *p;
		}

		FREE(old_buff);
	}
	else
		pnode->buff = ALLOC_NZ(caps[++pnode->cap_index], struct fix_tag);

	return YES;
}

struct fix_tag* add_fix_tag(struct fix_group_node* pnode, const struct fix_tag* new_tag)
{
	struct fix_tag* ptag;

	if(pnode->size >= (3 * caps[pnode->cap_index]) / 4 && !expand_message_node(pnode))
		return NULL;	// too many tags

	ptag = find_fix_tag(pnode, new_tag->tag);

	if(ptag->tag == new_tag->tag)	// duplicate
		return ptag;

	*ptag = *new_tag;
	++pnode->size;

	return ptag;
}

// FIX node interface -----------------------------------------------------------------------------
const struct fix_group_node* get_next_fix_node(const struct fix_group_node* pnode)
{
	return pnode->next;
}

const struct fix_tag* get_fix_tag(const struct fix_group_node* node, size_t tag)
{
	const struct fix_tag* const p = find_fix_tag(node, tag);

	return p && p->tag == tag ? p : NULL;
}

size_t get_fix_node_size(const struct fix_group_node* node)
{
	return node->size;
}

