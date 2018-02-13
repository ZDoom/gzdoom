/* $Id: tmpfileplus.h $ */
/*
 * $Date: 2016-06-01 03:31Z $
 * $Revision: 2.0.0 $
 * $Author: dai $
 */

/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2012-16 David Ireland, DI Management Services Pty Ltd
 * <http://www.di-mgt.com.au/contact/>.
 */

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef TMPFILEPLUS_H_
#define TMPFILEPLUS_H_

#include <stdio.h>

class FString;

/** Create a unique temporary file.
@param dir (optional) directory to create file. If NULL use default TMP directory.
@param prefix (optional) prefix for file name. If NULL use "tmp.".
@param pathname (optional) pointer to a buffer to receive the temp filename. 
	Allocated using `malloc()`; user to free. Ignored if NULL.
@param keep If `keep` is nonzero and `pathname` is not NULL, then keep the file after closing. 
	Otherwise file is automatically deleted when closed.
@return Pointer to stream opened in binary read/write (w+b) mode, or a null pointer on error.
@exception ENOMEM Not enough memory to allocate filename.
*/
FILE *tmpfileplus(const char *dir, const char *prefix, FString *pathname, int keep);


#define TMPFILE_KEEP 1

#endif /* end TMPFILEPLUS_H_ */
