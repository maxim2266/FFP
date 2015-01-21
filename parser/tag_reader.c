/*
Copyright (c) 2013, 2014, 2015, Maxim Konakov
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

#include <assert.h>

void init_tag_reader(struct fix_parser* parser, struct tag_reader* reader)
{
	assert(parser);
	assert(reader);

	reader->ptr = parser->buffer.str;
	reader->end = parser->buffer.str + parser->buffer.size;
	reader->has_unread_tag = NO;
	reader->recursion_level = 0u;
	reader->parser = parser;
}

#define ERROR_RETURN(code) return (reader->end = reader->ptr = NULL, code)

static
tag_reader_status read_tag(struct tag_reader* reader)
{
	char* s = reader->ptr;

	if(s >= reader->end)
		return TR_DONE;

	// tag
	s = (char*)read_fix_uint(s, reader->end, &reader->current.tag);

	if(!s || *s != '=' || ++s == reader->end)
	{
		report_message_error(reader->parser, "Invalid tag format");
		ERROR_RETURN(TR_ERROR);
	}

	reader->ptr = s;

	return TR_OK;
}

tag_reader_status read_next_tag(struct tag_reader* reader)
{
	char* s;
	tag_reader_status r;

	if(reader->ptr == NULL)
		return TR_ERROR;

	if(reader->has_unread_tag)
	{
		reader->has_unread_tag = NO;
		return TR_OK;
	}

	r = read_tag(reader);

	if(r != TR_OK)
		return r;

	//s = (char*)memchr(reader->ptr, SOH, reader->end - reader->ptr);
	for(s = reader->ptr; s < reader->end && *s != SOH; ++s);

	if(s == reader->end)
	{
		report_message_error(reader->parser, "Unexpected end of message");
		ERROR_RETURN(TR_ERROR);
	}

	reader->current.value = reader->ptr;
	reader->current.length = (size_t)(s - reader->ptr);

	if(reader->current.length == 0)
	{
		report_message_error(reader->parser, "Value for tag %u is missing", (unsigned)reader->current.tag);
		ERROR_RETURN(TR_ERROR);
	}

	*s++ = 0;	// replacing SOH
	reader->ptr = s;
	reader->current.group = NULL;

	return TR_OK;
}

tag_reader_status read_binary_tag(struct tag_reader* reader, size_t tag)
{
	size_t len;
	tag_reader_status r;

	if(reader->ptr == NULL)
		return TR_ERROR;

	if(!read_fix_uint(reader->current.value, reader->current.value + reader->current.length, &len))
	{
		report_message_error(reader->parser, "Invalid value length format for tag %u", (unsigned)reader->current.tag);
		ERROR_RETURN(TR_ERROR);
	}

	r = read_tag(reader);

	if(r != TR_OK)
		return r;

	if(reader->current.tag != tag)
	{
		report_message_error(reader->parser, "Expected tag %u, but got %u instead", (unsigned)tag, (unsigned)reader->current.tag);
		ERROR_RETURN(TR_ERROR);
	}

	if(len >= (size_t)(reader->end - reader->ptr) || *(reader->ptr + len) != SOH)
	{
		report_message_error(reader->parser, "Invalid value length %u for tag %u", (unsigned)len, (unsigned)reader->current.tag);
		ERROR_RETURN(TR_ERROR);
	}

	*(reader->ptr + len) = 0;	// replacing SOH
	reader->current.length = len;
	reader->current.value = reader->ptr;
	reader->current.group = NULL;
	reader->ptr += len + 1;

	return TR_OK;
}

