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
 * @file xlsxio_read.h
 * @brief XLSX I/O header file for reading .xlsx files.
 * @author Brecht Sanders
 * @date 2016
 * @copyright MIT
 *
 * Include this header file to use XLSX I/O for reading .xlsx files and
 * link with -lxlsxio_read.
 * This header provides both advanced methods using callback functions and
 * simple methods for iterating through data.
 */

#ifndef INCLUDED_XLSXIO_READ_H
#define INCLUDED_XLSXIO_READ_H

#include <stdlib.h>
#include <stdlib.h>
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef signed __int64 int64_t;
#else
#include <stdint.h>
#endif
#include <time.h>

#define STATIC

/*! \cond PRIVATE */
#ifndef DLL_EXPORT_XLSXIO
#ifdef _WIN32
#if defined(BUILD_XLSXIO_DLL)
#define DLL_EXPORT_XLSXIO __declspec(dllexport)
#elif !defined(STATIC) && !defined(BUILD_XLSXIO_STATIC) && !defined(BUILD_XLSXIO)
#define DLL_EXPORT_XLSXIO __declspec(dllimport)
#else
#define DLL_EXPORT_XLSXIO
#endif
#else
#define DLL_EXPORT_XLSXIO
#endif
#endif
/*! \endcond */

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief get xlsxio_write version string
 * \param  pmajor        pointer to integer that will receive major version number
 * \param  pminor        pointer to integer that will receive minor version number
 * \param  pmicro        pointer to integer that will receive micro version number
 * \sa     xlsxiowrite_get_version_string()
 */
DLL_EXPORT_XLSXIO void xlsxioread_get_version (int* pmajor, int* pminor, int* pmicro);

/*! \brief get xlsxio_write version string
 * \return version string
 * \sa     xlsxiowrite_get_version()
 */
DLL_EXPORT_XLSXIO const char* xlsxioread_get_version_string (void);

/*! \brief read handle for .xlsx object */
typedef struct xlsxio_read_struct* xlsxioreader;

/*! \brief open .xlsx file
 * \param  filename      path of .xlsx file to open
 * \return read handle for .xlsx object or NULL on error
 * \sa     xlsxioread_close()
 */
DLL_EXPORT_XLSXIO xlsxioreader xlsxioread_open (int lumpnum);

/*! \brief close .xlsx file
 * \param  handle        read handle for .xlsx object
 * \sa     xlsxioread_open()
 */
DLL_EXPORT_XLSXIO void xlsxioread_close (xlsxioreader handle);



/*! \brief type of pointer to callback function for listing worksheets
 * \param  name          name of worksheet
 * \param  callbackdata  callback data passed to xlsxioread_list_sheets
 * \return zero to continue, non-zero to abort
 * \sa     xlsxioread_list_sheets()
 */
typedef int (*xlsxioread_list_sheets_callback_fn)(const char* name, void* callbackdata);

/*! \brief list worksheets in .xlsx file
 * \param  handle        read handle for .xlsx object
 * \param  callback      callback function called for each worksheet
 * \param  callbackdata  custom data as passed to quickmail_add_body_custom/quickmail_add_attachment_custom
 * \sa     xlsxioread_list_sheets_callback_fn
 */
DLL_EXPORT_XLSXIO void xlsxioread_list_sheets (xlsxioreader handle, xlsxioread_list_sheets_callback_fn callback, void* callbackdata);



/*! \brief possible values for the flags parameter of xlsxioread_process()
 * \sa     xlsxioread_process()
 * \name   XLSXIOREAD_SKIP_*
 * \{
 */
/*! \brief don't skip any rows or cells \hideinitializer */
#define XLSXIOREAD_SKIP_NONE            0
/*! \brief skip empty rows (note: cells may appear empty while they actually contain data) \hideinitializer */
#define XLSXIOREAD_SKIP_EMPTY_ROWS      0x01
/*! \brief skip empty cells \hideinitializer */
#define XLSXIOREAD_SKIP_EMPTY_CELLS     0x02
/*! \brief skip empty rows and cells \hideinitializer */
#define XLSXIOREAD_SKIP_ALL_EMPTY       (XLSXIOREAD_SKIP_EMPTY_ROWS | XLSXIOREAD_SKIP_EMPTY_CELLS)
/*! \brief skip extra cells to the right of the rightmost header cell \hideinitializer */
#define XLSXIOREAD_SKIP_EXTRA_CELLS     0x04
/*! @} */

/*! \brief type of pointer to callback function for processing a worksheet cell value
 * \param  row           row number (first row is 1)
 * \param  col           column number (first column is 1)
 * \param  value         value of cell (note: formulas are not calculated)
 * \param  callbackdata  callback data passed to xlsxioread_process
 * \return zero to continue, non-zero to abort
 * \sa     xlsxioread_process()
 * \sa     xlsxioread_process_row_callback_fn
 */
typedef int (*xlsxioread_process_cell_callback_fn)(size_t row, size_t col, const char* value, void* callbackdata);

/*! \brief type of pointer to callback function for processing the end of a worksheet row
 * \param  row           row number (first row is 1)
 * \param  maxcol        maximum column number on this row (first column is 1)
 * \param  callbackdata  callback data passed to xlsxioread_process
 * \return zero to continue, non-zero to abort
 * \sa     xlsxioread_process()
 * \sa     xlsxioread_process_cell_callback_fn
 */
typedef int (*xlsxioread_process_row_callback_fn)(size_t row, size_t maxcol, void* callbackdata);

/*! \brief process all rows and columns of a worksheet in an .xlsx file
 * \param  handle        read handle for .xlsx object
 * \param  sheetname     worksheet name (NULL for first sheet)
 * \param  flags         XLSXIOREAD_SKIP_ flag(s) to determine how data is processed
 * \param  cell_callback callback function called for each cell
 * \param  row_callback  callback function called after each row
 * \param  callbackdata  callback data passed to xlsxioread_process
 * \return zero on success, non-zero on error
 * \sa     xlsxioread_process_row_callback_fn
 * \sa     xlsxioread_process_cell_callback_fn
 */
DLL_EXPORT_XLSXIO int xlsxioread_process (xlsxioreader handle, const char* sheetname, unsigned int flags, xlsxioread_process_cell_callback_fn cell_callback, xlsxioread_process_row_callback_fn row_callback, void* callbackdata);



/*! \brief read handle for list of worksheet names */
typedef struct xlsxio_read_sheetlist_struct* xlsxioreadersheetlist;

/*! \brief open list of worksheet names
 * \param  handle           read handle for .xlsx object
 * \sa     xlsxioread_sheetlist_close()
 * \sa     xlsxioread_open()
 */
DLL_EXPORT_XLSXIO xlsxioreadersheetlist xlsxioread_sheetlist_open (xlsxioreader handle);

/*! \brief close worksheet
 * \param  sheetlisthandle  read handle for worksheet object
 * \sa     xlsxioread_sheetlist_open()
 */
DLL_EXPORT_XLSXIO void xlsxioread_sheetlist_close (xlsxioreadersheetlist sheetlisthandle);

/*! \brief get next cell from worksheet
 * \param  sheetlisthandle  read handle for worksheet object
 * \return name of worksheet or NULL if no more worksheets are available
 * \sa     xlsxioread_sheetlist_open()
 */
DLL_EXPORT_XLSXIO const char* xlsxioread_sheetlist_next (xlsxioreadersheetlist sheetlisthandle);



/*! \brief read handle for worksheet object */
typedef struct xlsxio_read_sheet_struct* xlsxioreadersheet;

/*! \brief open worksheet
 * \param  handle        read handle for .xlsx object
 * \param  sheetname     worksheet name (NULL for first sheet)
 * \param  flags         XLSXIOREAD_SKIP_ flag(s) to determine how data is processed
 * \return read handle for worksheet object or NULL in case of error
 * \sa     xlsxioread_sheet_close()
 * \sa     xlsxioread_open()
 */
DLL_EXPORT_XLSXIO xlsxioreadersheet xlsxioread_sheet_open (xlsxioreader handle, const char* sheetname, unsigned int flags);

/*! \brief close worksheet
 * \param  handle        read handle for worksheet object
 * \sa     xlsxioread_sheet_open()
 */
DLL_EXPORT_XLSXIO void xlsxioread_sheet_close (xlsxioreadersheet sheethandle);

/*! \brief get next row from worksheet (to be called before each row)
 * \param  handle        read handle for worksheet object
 * \return non-zero if a new row is available
 * \sa     xlsxioread_sheet_open()
 */
DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_row (xlsxioreadersheet sheethandle);

/*! \brief get next cell from worksheet
 * \param  handle        read handle for worksheet object
 * \return value (caller must free the result) or NULL if no more cells are available in the current row
 * \sa     xlsxioread_sheet_open()
 */
DLL_EXPORT_XLSXIO char* xlsxioread_sheet_next_cell (xlsxioreadersheet sheethandle);

/*! \brief get next cell from worksheet as a string
 * \param  handle        read handle for worksheet object
 * \param  pvalue        pointer where string will be stored if data is available (caller must free the result)
 * \return non-zero if a new cell was available in the current row
 * \sa     xlsxioread_sheet_open()
 * \sa     xlsxioread_sheet_next_cell()
 */
DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_string (xlsxioreadersheet sheethandle, char** pvalue);

/*! \brief get next cell from worksheet as an integer
 * \param  handle        read handle for worksheet object
 * \param  pvalue        pointer where integer will be stored if data is available
 * \return non-zero if a new cell was available in the current row
 * \sa     xlsxioread_sheet_open()
 * \sa     xlsxioread_sheet_next_cell()
 */
DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_int (xlsxioreadersheet sheethandle, int64_t* pvalue);

/*! \brief get next cell from worksheet as a floating point value
 * \param  handle        read handle for worksheet object
 * \param  pvalue        pointer where floating point value will be stored if data is available
 * \return non-zero if a new cell was available in the current row
 * \sa     xlsxioread_sheet_open()
 * \sa     xlsxioread_sheet_next_cell()
 */
DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_float (xlsxioreadersheet sheethandle, double* pvalue);

/*! \brief get next cell from worksheet as date and time data
 * \param  handle        read handle for worksheet object
 * \param  pvalue        pointer where date and time data will be stored if data is available
 * \return non-zero if a new cell was available in the current row
 * \sa     xlsxioread_sheet_open()
 * \sa     xlsxioread_sheet_next_cell()
 */
DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_datetime (xlsxioreadersheet sheethandle, time_t* pvalue);

#ifdef __cplusplus
}
#endif

#endif
