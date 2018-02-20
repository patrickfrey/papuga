/*
 * Copyright (c) 2017 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* \brief Utilities for XML/JSON request parsers
 * \file requestParser_utils.c
 */
#include "requestParser_utils.h"
#include <stdbool.h>

#define B10000000 0x80
#define B11000000 0xC0

static bool isUTF8MidChar( unsigned char ch)
{
	return (ch >= B10000000 && (ch & B11000000) == B10000000);
}

void fillErrorLocation( char* errlocbuf, size_t errlocbufsize, const char* source, size_t errpos, const char* marker)
{
	size_t start = errpos > (errlocbufsize / 2) ? errpos - (errlocbufsize / 2) : 0;
	char const* cc = source + start;
	size_t ei, ee;

	while (isUTF8MidChar( *cc))
	{
		++cc;
		++start;
	}
	ei = 0, ee = errlocbufsize-1;
	for (; ei < ee && *cc; ++ei,++cc,++start)
	{
		if (start == errpos)
		{
			char const* mi = marker;
			for (; ei < ee && *mi; ++ei,++mi)
			{
				errlocbuf[ ei] = *mi;
			}
		}
		errlocbuf[ ei] = (unsigned char)*cc > 32 ? *cc : ' ';
	}
	while (ei>0 && isUTF8MidChar( errlocbuf[ ei-1]))
	{
		--ei;
	}
	errlocbuf[ei] = 0;
}


