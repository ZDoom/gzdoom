/*****************************************************************************
Copyright (C)  2016  Brecht Sanders  All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*****************************************************************************/

/**
 * @file xlsxio_version.h
 * @brief XLSX I/O header file with version information.
 * @author Brecht Sanders
 *
 * Only use this header file when version information is needed at compile
 * time. Otherwise the version functions in the libraries should be used.
 * \sa     XLSXIO_VERSION_*
 * \sa     XLSXIO_VERSION_STRING
 * \sa     xlsxioread_get_version()
 * \sa     xlsxioread_get_version_string()
 * \sa     xlsxiowrite_get_version()
 * \sa     xlsxiowrite_get_version_string()
 */

#ifndef INCLUDED_XLSXIO_VERSION_H
#define INCLUDED_XLSXIO_VERSION_H

/*! \brief possible values for the flags parameter of xlsxioread_process()
 * \sa     xlsxioread_get_version()
 * \sa     xlsxiowrite_get_version()
 * \name   XLSXIO_VERSION_*
 * \{
 */
/*! \brief major version number */
#define XLSXIO_VERSION_MAJOR 0
/*! \brief minor version number */
#define XLSXIO_VERSION_MINOR 2
/*! \brief micro version number */
#define XLSXIO_VERSION_MICRO 8
/*! @} */

/*! \cond PRIVATE */
#define XLSXIO_VERSION_STRINGIZE_(major, minor, micro) #major"."#minor"."#micro
#define XLSXIO_VERSION_STRINGIZE(major, minor, micro) XLSXIO_VERSION_STRINGIZE_(major, minor, micro)
/*! \endcond */

/*! \brief string with dotted version number \hideinitializer */
#define XLSXIO_VERSION_STRING XLSXIO_VERSION_STRINGIZE(XLSXIO_VERSION_MAJOR, XLSXIO_VERSION_MINOR, XLSXIO_VERSION_MICRO)

/*! \brief string with name of XLSX I/O reading library */
#define XLSXIOREAD_NAME "libxlsxio_read"
/*! \brief string with name of XLSX I/O writing library */
#define XLSXIOWRITE_NAME "libxlsxio_write"

/*! \brief string with name and version of XLSX I/O reading library \hideinitializer */
#define XLSXIOREAD_FULLNAME XLSXIOREAD_NAME " " XLSXIO_VERSION_STRING
/*! \brief string with name and version of XLSX I/O writing library \hideinitializer */
#define XLSXIOWRITE_FULLNAME XLSXIOWRITE_NAME " " XLSXIO_VERSION_STRING

#endif
