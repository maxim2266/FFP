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

#include <ctype.h>
#include <malloc.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

// parser error reporting
void set_parser_error(struct fix_parser* parser, const char* text, size_t n)
{
	copy_bytes_to_string_buffer(&parser->buffer, text, n);
	parser->error = parser->buffer.str;
	parser->message.complete = YES;
}

// message error reporting
static const char* const version_strings[] = { "FIX.4.2", "FIX.4.3", "FIX.4.4", "FIX.5.0" };

NOINLINE
void report_message_error(struct fix_parser* parser, const char* fmt, ...)
{
	static const char def_msg[] = "Unknown FIX message error (Invalid error message format)";

	int n;
	va_list args;
	char buff[1000];

	// message prefix (assuming it never fails)
	const int prefix_len = SPRINTF_S(buff, sizeof(buff), "FIX message (version '%s', type '%s') error: ", version_strings[parser->message.properties.version], parser->message.properties.type);

	// the rest of the message
	va_start(args, fmt);
	n = VSPRINTF_S(buff + prefix_len, sizeof(buff) - prefix_len, fmt, args);

	// copy bytes
	if(n > 0)
		copy_bytes_to_string_buffer(&parser->buffer, buff, prefix_len + n + 1);
	else
		copy_bytes_to_string_buffer(&parser->buffer, def_msg, sizeof(def_msg));

	// set message properties
	parser->message.properties.error = parser->buffer.str;
	parser->message.complete = YES;
}

// parser entry point
static
const struct fix_message* run_parser(struct fix_parser* parser)
{
	if(parser->message.complete)
	{   // reset parser
		set_buffer_empty(&parser->buffer);
		set_message_empty(&parser->message);
	}

	read_message(parser);	// call splitter entry point

	return (!parser->error && parser->message.complete) ? &parser->message.properties : NULL;
}

// parser -----------------------------------------------------------------------------------------
// parser data to pass around
struct parser_state
{
	struct fix_group_node* node;
	const struct fix_tag_classifier* classifier;
};

// ask reader to read the next tag; every binary "Len" tag gets silently replaced with its corresponding data tag
static
tag_reader_status get_next_tag(struct tag_reader* reader, struct parser_state* state)
{
	size_t t;

	switch(read_next_tag(reader))
	{
	case TR_ERROR:
		return TR_ERROR;

	case TR_DONE:
		t = state->classifier->get_data_tag(reader->current.tag);

		if(t != 0)
		{
			report_message_error(reader->parser, "Unexpected end of message while reading binary tag %u", (unsigned)t);
			return TR_ERROR;
		}
		else
			return TR_DONE;

	case TR_OK:
		t = state->classifier->get_data_tag(reader->current.tag);

		return (t != 0) ? read_binary_tag(reader, t) : TR_OK;

	default:	// should never get here
		report_message_error(reader->parser, "Unknown reader state");
		return TR_ERROR;
	}
}

// add the current tag to the current node, with error handling
static
struct fix_tag* add_current_tag(struct tag_reader* reader, struct fix_group_node* node)
{
	struct fix_tag* pt = add_fix_tag(node, &reader->current);

	if(!pt)
	{
		report_message_error(reader->parser, "Too many tags in a message node");
		return NULL;
	}

	if(reader->current.value != pt->value)
	{
		report_message_error(reader->parser, "Duplicate tag %u in a message node", (unsigned)pt->tag);
		return NULL;
	}

	return pt;
}

static
boolean read_group(struct tag_reader* reader, struct parser_state* state, const struct fix_tag_classifier* classifier);

static
boolean dispatch_tag(struct tag_reader* reader, struct parser_state* state)
{
	const struct fix_tag_classifier* const cl = state->classifier->get_group_classifier(reader->current.tag);

	if(cl)
	{
		if(++reader->recursion_level == MAX_GROUP_DEPTH)
		{
			report_message_error(reader->parser, "Maximum level of recursion has been reached");
			return NO;
		}

		return read_group(reader, state, cl);
	}
	else
		return add_current_tag(reader, state->node) ? YES : NO;
}

static
boolean read_node(struct tag_reader* reader, struct parser_state* state)
{
	tag_reader_status r;

	// first tag
	switch(get_next_tag(reader, state))
	{
	case TR_OK:
		break;
	case TR_DONE:
		report_message_error(reader->parser, "Unexpected end of message while reading a group node");
		// fall through
	default:
		return NO;
	}

	if(!state->classifier->is_first_in_group(reader->current.tag))
	{
		report_message_error(reader->parser, "Unexpected tag %u", reader->current.tag);
		return NO;
	}

	if(!add_current_tag(reader, state->node))
		return NO;

	// other tags
	for(r = get_next_tag(reader, state); r == TR_OK; r = get_next_tag(reader, state))
	{
		if(!state->classifier->is_valid_tag(reader->current.tag) || state->classifier->is_first_in_group(reader->current.tag))
		{
			reader->has_unread_tag = YES;
			return YES;
		}

		if(!dispatch_tag(reader, state))
			return NO;
	}

	return (r != TR_ERROR) ? YES : NO;
}

static
boolean read_group(struct tag_reader* reader, struct parser_state* state, const struct fix_tag_classifier* classifier)
{
	size_t node_count;
	struct fix_tag* group_tag;

	if(!read_fix_uint(reader->current.value, reader->current.value + reader->current.length, &node_count))
	{
		report_message_error(reader->parser, "Invalid group length format for tag %u", (unsigned)reader->current.tag);
		return NO;
	}

	reader->current.value = NULL;
	reader->current.length = node_count;
	group_tag = add_current_tag(reader, state->node);

	if(!group_tag)
		return NO;

	if(node_count > 0)
	{
		struct parser_state new_state;

		new_state.classifier = classifier;
		group_tag->group = new_state.node = alloc_group_node();

		if(!read_node(reader, &new_state))
			return NO;

		while(--node_count > 0)
		{
			new_state.node = new_state.node->next = alloc_group_node();

			if(!read_node(reader, &new_state))
				return NO;
		}
	}

	return YES;
}

static
void process_root_node(struct tag_reader* reader, struct parser_state* state)
{
	tag_reader_status r;

	for(r = get_next_tag(reader, state); r == TR_OK; r = get_next_tag(reader, state))
	{
		if(!state->classifier->is_valid_tag(reader->current.tag))
		{
			report_message_error(reader->parser, "Unexpected tag %u", (unsigned)reader->current.tag);
			break;
		}

		if(!dispatch_tag(reader, state))
			break;
	}
}

// splitter callback
void parse_message(struct fix_parser* parser)
{
	struct parser_state state;
	struct tag_reader reader;

	state.classifier = parser->get_classifier(parser->message.properties.version, parser->message.properties.type);

	if(!state.classifier)
	{
		report_message_error(parser, "Unrecognised message");
		return;
	}

	state.node = &parser->message.root;
	init_tag_reader(parser, &reader);

	process_root_node(&reader, &state);

	parser->message.complete = YES;
}

// FIX parser interface ---------------------------------------------------------------------------
struct fix_parser* create_fix_parser(classifier_func cf)
{
	struct fix_parser* const parser = ALLOC_Z(struct fix_parser);

	assert(cf);

	parser->get_classifier = cf;
	init_splitter(&parser->splitter);
	return parser;
}

void free_fix_parser(struct fix_parser* parser)
{
	if(parser)
	{
		clear_group_node(&parser->message.root);	// clear root node
		FREE(parser->buffer.str);					// clear buffer
		FREE(parser);								// free the parser
	}
}

const char* get_fix_parser_error(struct fix_parser* parser)
{
	return parser->error;
}

const struct fix_message* get_first_fix_message(struct fix_parser* parser, const void* bytes, size_t n)
{
	if(parser->error)	
		return NULL;	// parser is unusable

	if(parser->ptr != parser->end)	// some bytes left unprocessed
	{
		parser->error = "Invalid parser state";
		return NULL;
	}

	parser->ptr = (const char*)bytes;
	parser->end = parser->ptr + n;

	return run_parser(parser);
}

const struct fix_message* get_next_fix_message(struct fix_parser* parser)
{
	if(parser->error)	
		return NULL;	// parser is unusable

	return run_parser(parser);
}
