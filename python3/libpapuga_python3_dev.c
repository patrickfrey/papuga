/*
* Copyright (c) 2017 Patrick P. Frey
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
/// \brief Library implementation for Python (v3) bindings built by papuga
/// \file libpapuga_python3_dev.c

#include "papuga/lib/python3_dev.h"
#include "papuga/valueVariant.h"
#include "papuga/callResult.h"
#include "papuga/errors.h"
#include "papuga/serialization.h"
#include "papuga/hostObject.h"
#include "papuga/iterator.h"
#include "papuga/stack.h"
#include "papuga/hostObject.h"
#include "private/dll_tags.h"
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <inttypes.h>
#include <stdarg.h>
#include <Python.h>

DLL_PUBLIC void papuga_python_error( const char* msg, ...)
{
	char buf[ 2048];
	va_list ap;
	va_start(ap, msg);
	
	vsnprintf( buf, sizeof(buf), msg, ap);
	va_end(ap);
}

