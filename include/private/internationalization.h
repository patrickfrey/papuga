/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef _STRUS_INTERNATIONALIZATION_H_INCLUDED
#define _STRUS_INTERNATIONALIZATION_H_INCLUDED
#include <libintl.h>

#ifdef __cplusplus
#define _TXT(STRING) const_cast<const char*>(gettext(STRING))
#else
#define _TXT(STRING) gettext(STRING)
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*
* @brief Declare the message domain used by this package for the exception constructors declared in this module for gettext
*/
void papuga_initMessageTextDomain(void);

#ifdef __cplusplus
}
#endif
#endif


