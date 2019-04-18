/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
* @brief Helper functions for basic types
* @file typedefs.c
*/
#include "papuga/typedefs.h"

const char* papuga_Tag_name( papuga_Tag tg)
{
	static const char* tgnamear[5] = {"value","open","close","name",0};
	return tgnamear[tg];
}

