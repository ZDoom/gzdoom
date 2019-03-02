/*
Copyright (C)  2016  Brecht Sanders  All Rights Reserved
Copyright (C)  2019  Christoph Oelckers  All Rights Reserved

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
*/

// This file was modified to use GZDoom's zip management instead of pulling in another external library for it.

#include "xlsxio_read.h"
#include "xlsxio_version.h"
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "expat/expat.h"
#include "resourcefiles/file_zip.h"
#include "w_wad.h"

#if defined(_MSC_VER)
#undef DLL_EXPORT_XLSXIO
#define DLL_EXPORT_XLSXIO
#endif

#define zip_t FResourceFile
#define zip_file_t FileReader

// Wrapper fpr libzip interface to FResourceFile.
zip_t *zip_open(int lumpnum, const void *)
{
    auto rf = FResourceFile::OpenResourceFileFromLump(lumpnum, true, true);
	if (rf == nullptr) return nullptr;
	// This will open a non-Zip as a loose lump so before returning check if the content is an XLSX file.
	auto lump = rf->FindLump("[Content_Types].xml");
	if (lump == nullptr)
	{
		delete rf;
		return nullptr;
	}
	return rf;
}

void zip_close(zip_t *zipfile)
{
    if (zipfile) delete zipfile;
}

zip_file_t *zip_fopen(zip_t *zipfile, const char *filename)
{
    if (!zipfile || !filename) return NULL;
    auto lump = zipfile->FindLump(filename);
    if (!lump) return NULL;
    return new FileReader(std::move(lump->NewReader()));
}

void zip_fclose(zip_file_t *zippedfile)
{
    if (zippedfile) delete zippedfile;
}

int zip_fread(zip_file_t *zippedfile, void *buf, size_t len)
{
    if (!zippedfile) return -1;
    return (int)zippedfile->Read(buf, (long)len);
}

size_t zip_get_num_entries(zip_t *zipfile)
{
    if (!zipfile) return 0;
    return zipfile->LumpCount();
}

const char *zip_get_name(zip_t *zipfile, size_t index)
{
    auto lump = zipfile->GetLump((int)index);
    if (!lump) return NULL;
    return lump->FullName.GetChars();
}


#ifndef _WIN32
#define stricmp strcasecmp
#endif

DLL_EXPORT_XLSXIO void xlsxioread_get_version (int* pmajor, int* pminor, int* pmicro)
{
  if (pmajor)
    *pmajor = XLSXIO_VERSION_MAJOR;
  if (pminor)
    *pminor = XLSXIO_VERSION_MINOR;
  if (pmicro)
    *pmicro = XLSXIO_VERSION_MICRO;
}

DLL_EXPORT_XLSXIO const char* xlsxioread_get_version_string ()
{
  return XLSXIO_VERSION_STRING;
}

////////////////////////////////////////////////////////////////////////

#define BUFFER_SIZE 256
//#define BUFFER_SIZE 4

//process XML file contents
int expat_process_zip_file (zip_t* zip, const char* filename, XML_StartElementHandler start_handler, XML_EndElementHandler end_handler, XML_CharacterDataHandler data_handler, void* callbackdata, XML_Parser* xmlparser)
{
  zip_file_t* zipfile;
  XML_Parser parser;
  char buf[BUFFER_SIZE];
  int buflen;
  enum XML_Status status = XML_STATUS_ERROR;
  if ((zipfile = zip_fopen(zip, filename)) == NULL) {
    return -1;
  }
  parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, callbackdata);
  XML_SetElementHandler(parser, start_handler, end_handler);
  XML_SetCharacterDataHandler(parser, data_handler);
  if (xmlparser)
    *xmlparser = parser;
  while ((buflen = zip_fread(zipfile, buf, sizeof(buf))) >= 0) {
    if ((status = XML_Parse(parser, buf, buflen, (buflen < (int)sizeof(buf) ? 1 : 0))) == XML_STATUS_ERROR)
      break;
    if (xmlparser && status == XML_STATUS_SUSPENDED)
      return 0;
  }
  XML_ParserFree(parser);
  zip_fclose(zipfile);
  //return (status == XML_STATUS_ERROR != XML_ERROR_FINISHED ? 1 : 0);
  return 0;
}

XML_Parser expat_process_zip_file_suspendable (zip_file_t* zipfile, XML_StartElementHandler start_handler, XML_EndElementHandler end_handler, XML_CharacterDataHandler data_handler, void* callbackdata)
{
  XML_Parser result;
  if ((result = XML_ParserCreate(NULL)) != NULL) {
    XML_SetUserData(result, callbackdata);
    XML_SetElementHandler(result, start_handler, end_handler);
    XML_SetCharacterDataHandler(result, data_handler);
  }
  return result;
}

enum XML_Status expat_process_zip_file_resume (zip_file_t* zipfile, XML_Parser xmlparser)
{
  enum XML_Status status;
  status = XML_ResumeParser(xmlparser);
  if (status == XML_STATUS_SUSPENDED)
    return status;
  if (status == XML_STATUS_ERROR && XML_GetErrorCode(xmlparser) != XML_ERROR_NOT_SUSPENDED)
    return status;
  char buf[BUFFER_SIZE];
  int buflen;
  while ((buflen = zip_fread(zipfile, buf, sizeof(buf))) > 0) {
    if ((status = XML_Parse(xmlparser, buf, buflen, (buflen < (int)sizeof(buf) ? 1 : 0))) == XML_STATUS_ERROR)
      return status;
    if (status == XML_STATUS_SUSPENDED)
      return status;
  }
  return status;
}

//get expat attribute by name, returns NULL if not found
const XML_Char* get_expat_attr_by_name (const XML_Char** atts, const XML_Char* name)
{
  const XML_Char** p = atts;
  if (p) {
    while (*p) {
      if (stricmp(*p++, name) == 0)
        return *p;
      p++;
    }
  }
  return NULL;
}

//generate .rels filename, returns NULL on error, caller must free result
char* get_relationship_filename (const char* filename)
{
  char* result;
  size_t filenamelen = strlen(filename);
  if ((result = (char*)malloc(filenamelen + 12))) {
    size_t i = filenamelen;
    while (i > 0) {
      if (filename[i - 1] == '/')
        break;
      i--;
    }
    memcpy(result, filename, i);
    memcpy(result + i, "_rels/", 6);
    memcpy(result + i + 6, filename + i, filenamelen - i);
    strcpy(result + filenamelen + 6, ".rels");
  }
  return result;
}

//join basepath and filename (caller must free result)
char* join_basepath_filename (const char* basepath, const char* filename)
{
  char* result = NULL;
  if (filename && *filename) {
    size_t basepathlen = (basepath ? strlen(basepath) : 0);
    size_t filenamelen = strlen(filename);
    if ((result = (char*)malloc(basepathlen + filenamelen + 1)) != NULL) {
      if (basepathlen > 0)
        memcpy(result, basepath, basepathlen);
      memcpy(result + basepathlen, filename, filenamelen);
      result[basepathlen + filenamelen] = 0;
    }
  }
  return result;
}

//determine column number based on cell coordinate (e.g. "A1"), returns 1-based column number or 0 on error
size_t get_col_nr (const char* A1col)
{
  const char* p = A1col;
  size_t result = 0;
  if (p) {
    while (*p) {
      if (*p >= 'A' && *p <= 'Z')
        result = result * 26 + (*p - 'A') + 1;
      else if (*p >= 'a' && *p <= 'z')
        result = result * 26 + (*p - 'a') + 1;
      else if (*p >= '0' && *p <= '9' && p != A1col)
        return result;
      else
        break;
      p++;
    }
  }
  return 0;
}

//determine row number based on cell coordinate (e.g. "A1"), returns 1-based row number or 0 on error
size_t get_row_nr (const char* A1col)
{
  const char* p = A1col;
  size_t result = 0;
  if (p) {
    while (*p) {
      if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
        ;
      else if (*p >= '0' && *p <= '9' && p != A1col)
        result = result * 10 + (*p - '0');
      else
        return 0;
      p++;
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////

struct sharedstringlist {
  char** strings;
  size_t count;
};

struct sharedstringlist* sharedstringlist_create ()
{
  struct sharedstringlist* sharedstrings;
  if ((sharedstrings = (struct sharedstringlist*)malloc(sizeof(struct sharedstringlist))) != NULL) {
    sharedstrings->strings = NULL;
    sharedstrings->count = 0;
  }
  return sharedstrings;
}

void sharedstringlist_destroy (struct sharedstringlist* sharedstrings)
{
  if (sharedstrings) {
    size_t i;
    for (i = 0; i < sharedstrings->count; i++)
      free(sharedstrings->strings[i]);
    free(sharedstrings);
  }
}

size_t sharedstringlist_size (struct sharedstringlist* sharedstrings)
{
  if (!sharedstrings)
    return 0;
  return sharedstrings->count;
}

int sharedstringlist_add_buffer (struct sharedstringlist* sharedstrings, const char* data, size_t datalen)
{
  char* s;
  char** p;
  if (!sharedstrings)
    return 1;
  if (!data) {
    s = NULL;
  } else {
    if ((s = (char*)malloc(datalen + 1)) == NULL)
      return 2;
    memcpy(s, data, datalen);
    s[datalen] = 0;
  }
  if ((p = (char**)realloc(sharedstrings->strings, (sharedstrings->count + 1) * sizeof(sharedstrings->strings[0]))) == NULL)
    return 3;
  sharedstrings->strings = p;
  sharedstrings->strings[sharedstrings->count++] = s;
  return 0;
}

int sharedstringlist_add_string (struct sharedstringlist* sharedstrings, const char* data)
{
  return sharedstringlist_add_buffer(sharedstrings, data, (data ? strlen(data) : 0));
}

const char* sharedstringlist_get (struct sharedstringlist* sharedstrings, size_t index)
{
  if (!sharedstrings || index >= sharedstrings->count)
    return NULL;
  return sharedstrings->strings[index];
}

////////////////////////////////////////////////////////////////////////

struct shared_strings_callback_data {
  XML_Parser xmlparser;
  zip_file_t* zipfile;
  struct sharedstringlist* sharedstrings;
  int insst;
  int insi;
  int intext;
  char* text;
  size_t textlen;
  char* skiptag;                        //tag to skip
  size_t skiptagcount;                  //nesting level for current tag to skip
  XML_StartElementHandler skip_start;   //start handler to set after skipping
  XML_EndElementHandler skip_end;       //end handler to set after skipping
  XML_CharacterDataHandler skip_data;   //data handler to set after skipping
};

void shared_strings_callback_data_initialize (struct shared_strings_callback_data* data, struct sharedstringlist* sharedstrings)
{
  data->xmlparser = NULL;
  data->zipfile = NULL;
  data->sharedstrings = sharedstrings;
  data->insst = 0;
  data->insi = 0;
  data->intext = 0;
  data->text = NULL;
  data->textlen = 0;
  data->skiptag = NULL;
  data->skiptagcount = 0;
  data->skip_start = NULL;
  data->skip_end = NULL;
  data->skip_data = NULL;
}

void shared_strings_callback_data_cleanup (struct shared_strings_callback_data* data)
{
  free(data->text);
  free(data->skiptag);
}

void shared_strings_callback_skip_tag_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (name && stricmp(name, data->skiptag) == 0) {
    //increment nesting level
    data->skiptagcount++;
  }
}

void shared_strings_callback_skip_tag_end (void* callbackdata, const XML_Char* name)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (!name || stricmp(name, data->skiptag) == 0) {
    if (--data->skiptagcount == 0) {
      //restore handlers when done skipping
      XML_SetElementHandler(data->xmlparser, data->skip_start, data->skip_end);
      XML_SetCharacterDataHandler(data->xmlparser, data->skip_data);
      free(data->skiptag);
      data->skiptag = NULL;
    }
  }
}

void shared_strings_callback_find_sharedstringtable_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void shared_strings_callback_find_sharedstringtable_end (void* callbackdata, const XML_Char* name);
void shared_strings_callback_find_shared_stringitem_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void shared_strings_callback_find_shared_stringitem_end (void* callbackdata, const XML_Char* name);
void shared_strings_callback_find_shared_string_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void shared_strings_callback_find_shared_string_end (void* callbackdata, const XML_Char* name);
void shared_strings_callback_string_data (void* callbackdata, const XML_Char* buf, int buflen);

void shared_strings_callback_find_sharedstringtable_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (stricmp(name, "sst") == 0) {
    XML_SetElementHandler(data->xmlparser, shared_strings_callback_find_shared_stringitem_start, NULL);
  }
}

void shared_strings_callback_find_sharedstringtable_end (void* callbackdata, const XML_Char* name)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (stricmp(name, "sst") == 0) {
    XML_SetElementHandler(data->xmlparser, shared_strings_callback_find_sharedstringtable_start, NULL);
  }
}

void shared_strings_callback_find_shared_stringitem_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (stricmp(name, "si") == 0) {
    if (data->text)
      free(data->text);
    data->text = NULL;
    data->textlen = 0;
    XML_SetElementHandler(data->xmlparser, shared_strings_callback_find_shared_string_start, shared_strings_callback_find_sharedstringtable_end);
  }
}

void shared_strings_callback_find_shared_stringitem_end (void* callbackdata, const XML_Char* name)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (stricmp(name, "si") == 0) {
    sharedstringlist_add_buffer(data->sharedstrings, data->text, data->textlen);
    if (data->text)
      free(data->text);
    data->text = NULL;
    data->textlen = 0;
    XML_SetElementHandler(data->xmlparser, shared_strings_callback_find_shared_stringitem_start, shared_strings_callback_find_sharedstringtable_end);
  } else {
    shared_strings_callback_find_sharedstringtable_end(callbackdata, name);
  }
}

void shared_strings_callback_find_shared_string_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (stricmp(name, "t") == 0) {
    XML_SetElementHandler(data->xmlparser, NULL, shared_strings_callback_find_shared_string_end);
    XML_SetCharacterDataHandler(data->xmlparser, shared_strings_callback_string_data);
  } else if (stricmp(name, "rPh") == 0) {
    data->skiptag = strdup(name);
    data->skiptagcount = 1;
    data->skip_start = shared_strings_callback_find_shared_string_start;
    data->skip_end = shared_strings_callback_find_shared_stringitem_end;
    data->skip_data = NULL;
    XML_SetElementHandler(data->xmlparser, shared_strings_callback_skip_tag_start, shared_strings_callback_skip_tag_end);
    XML_SetCharacterDataHandler(data->xmlparser, NULL);
  }
}

void shared_strings_callback_find_shared_string_end (void* callbackdata, const XML_Char* name)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if (stricmp(name, "t") == 0) {
    XML_SetElementHandler(data->xmlparser, shared_strings_callback_find_shared_string_start, shared_strings_callback_find_shared_stringitem_end);
    XML_SetCharacterDataHandler(data->xmlparser, NULL);
  } else {
    shared_strings_callback_find_shared_stringitem_end(callbackdata, name);
  }
}

void shared_strings_callback_string_data (void* callbackdata, const XML_Char* buf, int buflen)
{
  struct shared_strings_callback_data* data = (struct shared_strings_callback_data*)callbackdata;
  if ((data->text = (char*)realloc(data->text, data->textlen + buflen)) == NULL) {
    //memory allocation error
    data->textlen = 0;
  } else {
    memcpy(data->text + data->textlen, buf, buflen);
    data->textlen += buflen;
  }
}

////////////////////////////////////////////////////////////////////////

struct xlsxio_read_struct {
  zip_t* zip;
};

DLL_EXPORT_XLSXIO xlsxioreader xlsxioread_open (int lumpnum)
{
  xlsxioreader result;
  if ((result = (xlsxioreader)malloc(sizeof(struct xlsxio_read_struct))) != NULL) {
    if ((result->zip = zip_open(lumpnum, NULL)) == NULL) {
      free(result);
      return NULL;
    }
  }
  return result;
}

DLL_EXPORT_XLSXIO void xlsxioread_close (xlsxioreader handle)
{
  if (handle) {
    zip_close(handle->zip);
    free(handle);
  }
}

////////////////////////////////////////////////////////////////////////

//callback function definition for use with list_files_by_contenttype
typedef void (*contenttype_file_callback_fn)(zip_t* zip, const char* filename, const char* contenttype, void* callbackdata);

struct list_files_by_contenttype_callback_data {
  zip_t* zip;
  const char* contenttype;
  contenttype_file_callback_fn filecallbackfn;
  void* filecallbackdata;
};

//expat callback function for element start used by list_files_by_contenttype
void list_files_by_contenttype_expat_callback_element_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct list_files_by_contenttype_callback_data* data = (struct list_files_by_contenttype_callback_data*)callbackdata;
  if (stricmp(name, "Override") == 0) {
    //explicitly specified file
    const XML_Char* contenttype;
    const XML_Char* partname;
    if ((contenttype = get_expat_attr_by_name(atts, "ContentType")) != NULL && stricmp(contenttype, data->contenttype) == 0) {
      if ((partname = get_expat_attr_by_name(atts, "PartName")) != NULL) {
        if (partname[0] == '/')
          partname++;
        data->filecallbackfn(data->zip, partname, contenttype, data->filecallbackdata);
      }
    }
  } else if (stricmp(name, "Default") == 0) {
    //by extension
    const XML_Char* contenttype;
    const XML_Char* extension;
    if ((contenttype = get_expat_attr_by_name(atts, "ContentType")) != NULL && stricmp(contenttype, data->contenttype) == 0) {
      if ((extension = get_expat_attr_by_name(atts, "Extension")) != NULL) {
        const char* filename;
        size_t filenamelen;
        size_t i;
        size_t zipnumfiles = zip_get_num_entries(data->zip);
        size_t extensionlen = strlen(extension);
        for (i = 0; i < zipnumfiles; i++) {
          filename = zip_get_name(data->zip, i);
          filenamelen = strlen(filename);
          if (filenamelen > extensionlen && filename[filenamelen - extensionlen - 1] == '.' && stricmp(filename + filenamelen - extensionlen, extension) == 0) {
            data->filecallbackfn(data->zip, filename, contenttype, data->filecallbackdata);
          }
        }
      }
    }
  }
}

//list file names by content type
int list_files_by_contenttype (zip_t* zip, const char* contenttype, contenttype_file_callback_fn filecallbackfn, void* filecallbackdata, XML_Parser* xmlparser)
{
  struct list_files_by_contenttype_callback_data callbackdata = {
    zip,
    contenttype,
    filecallbackfn,
    filecallbackdata
  };
  return expat_process_zip_file(zip, "[Content_Types].xml", list_files_by_contenttype_expat_callback_element_start, NULL, NULL, &callbackdata, xmlparser);
}

////////////////////////////////////////////////////////////////////////

//callback structure used by main_sheet_list_expat_callback_element_start
struct main_sheet_list_callback_data {
  XML_Parser xmlparser;
  xlsxioread_list_sheets_callback_fn callback;
  void* callbackdata;
};

//callback used by xlsxioread_list_sheets
void main_sheet_list_expat_callback_element_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct main_sheet_list_callback_data* data = (struct main_sheet_list_callback_data*)callbackdata;
  if (data && data->callback) {
    if (stricmp(name, "sheet") == 0) {
      const XML_Char* sheetname;
      //const XML_Char* relid = get_expat_attr_by_name(atts, "r:id");
      if ((sheetname = get_expat_attr_by_name(atts, "name")) != NULL) {
        if (data->callback) {
          if ((*data->callback)(sheetname, data->callbackdata) != 0) {
            XML_StopParser(data->xmlparser, XML_FALSE);
            return;
          }
/*
        } else {
          //for non-calback method suspend here
          XML_StopParser(data->xmlparser, XML_TRUE);
*/
        }
      }
    }
  }
}

//process contents each sheet listed in main sheet
void xlsxioread_list_sheets_callback (zip_t* zip, const char* filename, const char* contenttype, void* callbackdata)
{
  //get sheet information from file
  expat_process_zip_file(zip, filename, main_sheet_list_expat_callback_element_start, NULL, NULL, callbackdata, &((struct main_sheet_list_callback_data*)callbackdata)->xmlparser);
}

//list all worksheets
DLL_EXPORT_XLSXIO void xlsxioread_list_sheets (xlsxioreader handle, xlsxioread_list_sheets_callback_fn callback, void* callbackdata)
{
  if (!handle || !callback)
    return;
  //process contents of main sheet
  struct main_sheet_list_callback_data sheetcallbackdata = {
    NULL,
    callback,
    callbackdata
  };
  list_files_by_contenttype(handle->zip, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml", xlsxioread_list_sheets_callback, &sheetcallbackdata, &sheetcallbackdata.xmlparser);
}

////////////////////////////////////////////////////////////////////////

//callback data structure used by main_sheet_get_sheetfile_callback
struct main_sheet_get_rels_callback_data {
  XML_Parser xmlparser;
  const char* sheetname;
  char* basepath;
  char* sheetrelid;
  char* sheetfile;
  char* sharedstringsfile;
};

//determine relationship id for specific sheet name
void main_sheet_get_relid_expat_callback_element_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct main_sheet_get_rels_callback_data* data = (struct main_sheet_get_rels_callback_data*)callbackdata;
  if (stricmp(name, "sheet") == 0) {
    const XML_Char* name = get_expat_attr_by_name(atts, "name");
    if (!data->sheetname || stricmp(name, data->sheetname) == 0) {
      const XML_Char* relid = get_expat_attr_by_name(atts, "r:id");
      if (relid && *relid) {
        data->sheetrelid = strdup(relid);
        XML_StopParser(data->xmlparser, XML_FALSE);
        return;
      }
    }
  }
}

//determine sheet file name for specific relationship id
void main_sheet_get_sheetfile_expat_callback_element_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct main_sheet_get_rels_callback_data* data = (struct main_sheet_get_rels_callback_data*)callbackdata;
  if (data->sheetrelid) {
    if (stricmp(name, "Relationship") == 0) {
      const XML_Char* reltype;
      if ((reltype = get_expat_attr_by_name(atts, "Type")) != NULL && stricmp(reltype, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet") == 0) {
        const XML_Char* relid = get_expat_attr_by_name(atts, "Id");
        if (stricmp(relid, data->sheetrelid) == 0) {
          const XML_Char* filename = get_expat_attr_by_name(atts, "Target");
          if (filename && *filename) {
            data->sheetfile = join_basepath_filename(data->basepath, filename);
          }
        }
      } else if ((reltype = get_expat_attr_by_name(atts, "Type")) != NULL && stricmp(reltype, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings") == 0) {
        const XML_Char* filename = get_expat_attr_by_name(atts, "Target");
        if (filename && *filename) {
          data->sharedstringsfile = join_basepath_filename(data->basepath, filename);
        }
      }
    }
  }
}

//determine the file name for a specified sheet name
void main_sheet_get_sheetfile_callback (zip_t* zip, const char* filename, const char* contenttype, void* callbackdata)
{
  struct main_sheet_get_rels_callback_data* data = (struct main_sheet_get_rels_callback_data*)callbackdata;
  if (!data->sheetrelid) {
    expat_process_zip_file(zip, filename, main_sheet_get_relid_expat_callback_element_start, NULL, NULL, callbackdata, &data->xmlparser);
  }
  if (data->sheetrelid) {
    char* relfilename;
    //determine base name (including trailing slash)
    size_t i = strlen(filename);
    while (i > 0) {
      if (filename[i - 1] == '/')
        break;
      i--;
    }
    if (data->basepath)
      free(data->basepath);
    if ((data->basepath = (char*)malloc(i + 1)) != NULL) {
      memcpy(data->basepath, filename, i);
      data->basepath[i] = 0;
    }
    //find sheet filename in relationship contents
    if ((relfilename = get_relationship_filename(filename)) != NULL) {
      expat_process_zip_file(zip, relfilename, main_sheet_get_sheetfile_expat_callback_element_start, NULL, NULL, callbackdata, &data->xmlparser);
      free(relfilename);
    } else {
      free(data->sheetrelid);
      data->sheetrelid = NULL;
    }
  }
}

////////////////////////////////////////////////////////////////////////

typedef enum {
  none,
  value_string,
  inline_string,
  shared_string
} cell_string_type_enum;

#define XLSXIOREAD_NO_CALLBACK          0x80

struct data_sheet_callback_data {
  XML_Parser xmlparser;
  struct sharedstringlist* sharedstrings;
  size_t rownr;
  size_t colnr;
  size_t cols;
  char* celldata;
  size_t celldatalen;
  cell_string_type_enum cell_string_type;
  unsigned int flags;
  char* skiptag;                        //tag to skip
  size_t skiptagcount;                  //nesting level for current tag to skip
  XML_StartElementHandler skip_start;   //start handler to set after skipping
  XML_EndElementHandler skip_end;       //end handler to set after skipping
  XML_CharacterDataHandler skip_data;   //data handler to set after skipping
  xlsxioread_process_row_callback_fn sheet_row_callback;
  xlsxioread_process_cell_callback_fn sheet_cell_callback;
  void* callbackdata;
};

void data_sheet_callback_data_initialize (struct data_sheet_callback_data* data, struct sharedstringlist* sharedstrings, unsigned int flags, xlsxioread_process_cell_callback_fn cell_callback, xlsxioread_process_row_callback_fn row_callback, void* callbackdata)
{
  data->xmlparser = NULL;
  data->sharedstrings = sharedstrings;
  data->rownr = 0;
  data->colnr = 0;
  data->cols = 0;
  data->celldata = NULL;
  data->celldatalen = 0;
  data->cell_string_type = none;
  data->flags = flags;
  data->skiptag = NULL;
  data->skiptagcount = 0;
  data->skip_start = NULL;
  data->skip_end = NULL;
  data->skip_data = NULL;
  data->sheet_cell_callback = cell_callback;
  data->sheet_row_callback = row_callback;
  data->callbackdata = callbackdata;
}

void data_sheet_callback_data_cleanup (struct data_sheet_callback_data* data)
{
  sharedstringlist_destroy(data->sharedstrings);
  free(data->celldata);
  free(data->skiptag);
}

void data_sheet_expat_callback_skip_tag_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (name && stricmp(name, data->skiptag) == 0) {
    //increment nesting level
    data->skiptagcount++;
  }
}

void data_sheet_expat_callback_skip_tag_end (void* callbackdata, const XML_Char* name)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (!name || stricmp(name, data->skiptag) == 0) {
    if (--data->skiptagcount == 0) {
      //restore handlers when done skipping
      XML_SetElementHandler(data->xmlparser, data->skip_start, data->skip_end);
      XML_SetCharacterDataHandler(data->xmlparser, data->skip_data);
      free(data->skiptag);
      data->skiptag = NULL;
    }
  }
}

void data_sheet_expat_callback_find_worksheet_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void data_sheet_expat_callback_find_worksheet_end (void* callbackdata, const XML_Char* name);
void data_sheet_expat_callback_find_sheetdata_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void data_sheet_expat_callback_find_sheetdata_end (void* callbackdata, const XML_Char* name);
void data_sheet_expat_callback_find_row_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void data_sheet_expat_callback_find_row_end (void* callbackdata, const XML_Char* name);
void data_sheet_expat_callback_find_cell_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void data_sheet_expat_callback_find_cell_end (void* callbackdata, const XML_Char* name);
void data_sheet_expat_callback_find_value_start (void* callbackdata, const XML_Char* name, const XML_Char** atts);
void data_sheet_expat_callback_find_value_end (void* callbackdata, const XML_Char* name);
void data_sheet_expat_callback_value_data (void* callbackdata, const XML_Char* buf, int buflen);
void data_sheet_expat_callback_find_worksheet_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "worksheet") == 0) {
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_sheetdata_start, NULL);
  }
}

void data_sheet_expat_callback_find_worksheet_end (void* callbackdata, const XML_Char* name)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "worksheet") == 0) {
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_worksheet_start, NULL);
  }
}

void data_sheet_expat_callback_find_sheetdata_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "sheetData") == 0) {
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_row_start, data_sheet_expat_callback_find_sheetdata_end);
  }
}

void data_sheet_expat_callback_find_sheetdata_end (void* callbackdata, const XML_Char* name)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "sheetData") == 0) {
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_sheetdata_start, data_sheet_expat_callback_find_worksheet_end);
  } else {
    data_sheet_expat_callback_find_worksheet_end(callbackdata, name);
  }
}

void data_sheet_expat_callback_find_row_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "row") == 0) {
    const XML_Char* hidden = get_expat_attr_by_name(atts, "hidden");
    if (!hidden || atoi(hidden) == 0) {//nesting level for current tag to skip
//start handler to set after skipping
//end handler to set after skipping
//data handler to set after skipping

      data->rownr++;
      data->colnr = 0;
      XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_cell_start, data_sheet_expat_callback_find_row_end);
      //for non-calback method suspend here on new row
      if (data->flags & XLSXIOREAD_NO_CALLBACK) {
        XML_StopParser(data->xmlparser, XML_TRUE);
      }
    } else {
      //skip hidden tow
      XML_SetElementHandler(data->xmlparser, NULL, data_sheet_expat_callback_find_row_end);
    }
  }
}

void data_sheet_expat_callback_find_row_end (void* callbackdata, const XML_Char* name)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "row") == 0) {
    //determine number of columns based on first row
    if (data->rownr == 1 && data->cols == 0)
      data->cols = data->colnr;
    //add empty columns if needed
    if (!(data->flags & XLSXIOREAD_NO_CALLBACK) && data->sheet_cell_callback && !(data->flags & XLSXIOREAD_SKIP_EMPTY_CELLS)) {
      while (data->colnr < data->cols) {
        if ((*data->sheet_cell_callback)(data->rownr, data->colnr + 1, NULL, data->callbackdata)) {
          XML_StopParser(data->xmlparser, XML_FALSE);
          return;
        }
        data->colnr++;
      }
    }
    free(data->celldata);
    data->celldata = NULL;
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_row_start, data_sheet_expat_callback_find_sheetdata_end);
    //process end of row
    if (!(data->flags & XLSXIOREAD_NO_CALLBACK)) {
      if (data->sheet_row_callback) {
        if ((*data->sheet_row_callback)(data->rownr, data->colnr, data->callbackdata)) {
          XML_StopParser(data->xmlparser, XML_FALSE);
          return;
        }
      }
    } else {
      //for non-calback method suspend here on end of row
      if (data->flags & XLSXIOREAD_NO_CALLBACK) {
        XML_StopParser(data->xmlparser, XML_TRUE);
      }
    }
  } else {
    data_sheet_expat_callback_find_sheetdata_end(callbackdata, name);
  }
}

void data_sheet_expat_callback_find_cell_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "c") == 0) {
    const XML_Char* t = get_expat_attr_by_name(atts, "r");
    size_t cellcolnr = get_col_nr(t);
    //skip everything when out of bounds
    if (cellcolnr && data->cols && (data->flags & XLSXIOREAD_SKIP_EXTRA_CELLS) && cellcolnr > data->cols) {
      data->colnr = cellcolnr - 1;
      return;
    }
    //insert empty rows if needed
    if (data->colnr == 0) {
      size_t cellrownr = get_row_nr(t);
      if (cellrownr) {
        if (!(data->flags & XLSXIOREAD_SKIP_EMPTY_ROWS) && !(data->flags & XLSXIOREAD_NO_CALLBACK)) {
          while (data->rownr < cellrownr) {
            //insert empty columns
            if (!(data->flags & XLSXIOREAD_SKIP_EMPTY_CELLS) && data->sheet_cell_callback) {
              while (data->colnr < data->cols) {
                if ((*data->sheet_cell_callback)(data->rownr, data->colnr + 1, NULL, data->callbackdata)) {
                  XML_StopParser(data->xmlparser, XML_FALSE);
                  return;
                }
                data->colnr++;
              }
            }
            //finish empty row
            if (data->sheet_row_callback) {
              if ((*data->sheet_row_callback)(data->rownr, data->cols, data->callbackdata)) {
                XML_StopParser(data->xmlparser, XML_FALSE);
                return;
              }
            }
            data->rownr++;
            data->colnr = 0;
          }
        } else {
          data->rownr = cellrownr;
        }
      }
    }
    //insert empty columns if needed
    if (cellcolnr) {
      cellcolnr--;
      if (data->flags & XLSXIOREAD_SKIP_EMPTY_CELLS || data->flags & XLSXIOREAD_NO_CALLBACK) {
        data->colnr = cellcolnr;
      } else {
        while (data->colnr < cellcolnr) {
          if (data->sheet_cell_callback) {
            if ((*data->sheet_cell_callback)(data->rownr, data->colnr + 1, NULL, data->callbackdata)) {
              XML_StopParser(data->xmlparser, XML_FALSE);
              return;
            }
          }
          data->colnr++;
        }
      }
    }
    //determing value type
    if ((t = get_expat_attr_by_name(atts, "t")) != NULL && stricmp(t, "s") == 0)
      data->cell_string_type = shared_string;
    else
      data->cell_string_type = value_string;
    //prepare empty value data
    free(data->celldata);
    data->celldata = NULL;
    data->celldatalen = 0;
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_value_start, data_sheet_expat_callback_find_cell_end);
  }
}

void data_sheet_expat_callback_find_cell_end (void* callbackdata, const XML_Char* name)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "c") == 0) {
    //determine value
    if (data->celldata) {
      const char* s = NULL;
      data->celldata[data->celldatalen] = 0;
      if (data->cell_string_type == shared_string) {
        //get shared string
        char* p = NULL;
        long num = strtol(data->celldata, &p, 10);
        if (!p || (p != data->celldata && *p == 0)) {
          s = sharedstringlist_get(data->sharedstrings, num);
          free(data->celldata);
          data->celldata = (s ? strdup(s) : NULL);
        }
      } else if (data->cell_string_type == none) {
        //unknown value type
        free(data->celldata);
        data->celldata = NULL;
      }
    }
    //reset data
    data->colnr++;
    data->cell_string_type = none;
    data->celldatalen = 0;
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_cell_start, data_sheet_expat_callback_find_row_end);
    XML_SetCharacterDataHandler(data->xmlparser, NULL);
    //process data if needed
    if (!(data->cols && (data->flags & XLSXIOREAD_SKIP_EXTRA_CELLS) && data->colnr > data->cols)) {
      //process data
      if (!(data->flags & XLSXIOREAD_NO_CALLBACK)) {
        if (data->sheet_cell_callback) {
          if ((*data->sheet_cell_callback)(data->rownr, data->colnr, data->celldata, data->callbackdata)) {
            XML_StopParser(data->xmlparser, XML_FALSE);
            return;
          }
        }
      } else {
        //for non-calback method suspend here with cell data
        if (!data->celldata)
          data->celldata = strdup("");
        XML_StopParser(data->xmlparser, XML_TRUE);
      }
    }
  } else {
    data_sheet_expat_callback_find_row_end(callbackdata, name);
  }
}

void data_sheet_expat_callback_find_value_start (void* callbackdata, const XML_Char* name, const XML_Char** atts)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "v") == 0 || stricmp(name, "t") == 0) {
    XML_SetElementHandler(data->xmlparser, NULL, data_sheet_expat_callback_find_value_end);
    XML_SetCharacterDataHandler(data->xmlparser, data_sheet_expat_callback_value_data);
  } else if (stricmp(name, "is") == 0) {
    data->cell_string_type = inline_string;
  } else if (stricmp(name, "rPh") == 0) {
    data->skiptag = strdup(name);
    data->skiptagcount = 1;
    data->skip_start = data_sheet_expat_callback_find_value_start;
    data->skip_end = data_sheet_expat_callback_find_cell_end;
    data->skip_data = NULL;
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_skip_tag_start, data_sheet_expat_callback_skip_tag_end);
    XML_SetCharacterDataHandler(data->xmlparser, NULL);
  }
}

void data_sheet_expat_callback_find_value_end (void* callbackdata, const XML_Char* name)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (stricmp(name, "v") == 0 || stricmp(name, "t") == 0) {
    XML_SetElementHandler(data->xmlparser, data_sheet_expat_callback_find_value_start, data_sheet_expat_callback_find_cell_end);
    XML_SetCharacterDataHandler(data->xmlparser, NULL);
  } else if (stricmp(name, "is") == 0) {
    data->cell_string_type = none;
  } else {
    data_sheet_expat_callback_find_row_end(callbackdata, name);
  }
}

void data_sheet_expat_callback_value_data (void* callbackdata, const XML_Char* buf, int buflen)
{
  struct data_sheet_callback_data* data = (struct data_sheet_callback_data*)callbackdata;
  if (data->cell_string_type != none) {
    if ((data->celldata = (char*)realloc(data->celldata, data->celldatalen + buflen + 1)) == NULL) {
      //memory allocation error
      data->celldatalen = 0;
    } else {
      //add new data to value buffer
      memcpy(data->celldata + data->celldatalen, buf, buflen);
      data->celldatalen += buflen;
    }
  }
}

////////////////////////////////////////////////////////////////////////

struct xlsxio_read_sheet_struct {
  xlsxioreader handle;
  zip_file_t* zipfile;
  struct data_sheet_callback_data processcallbackdata;
  size_t lastrownr;
  size_t paddingrow;
  size_t lastcolnr;
  size_t paddingcol;
};

DLL_EXPORT_XLSXIO int xlsxioread_process (xlsxioreader handle, const char* sheetname, unsigned int flags, xlsxioread_process_cell_callback_fn cell_callback, xlsxioread_process_row_callback_fn row_callback, void* callbackdata)
{
  int result = 0;
  //determine sheet file name
  struct main_sheet_get_rels_callback_data getrelscallbackdata = {
	  NULL,
    sheetname,
    NULL,
    NULL,
    NULL,
    NULL
  };
  list_files_by_contenttype(handle->zip, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml", main_sheet_get_sheetfile_callback, &getrelscallbackdata, NULL);

  //process shared strings
  struct sharedstringlist* sharedstrings = sharedstringlist_create();
  struct shared_strings_callback_data sharedstringsdata;
  shared_strings_callback_data_initialize(&sharedstringsdata, sharedstrings);
  if (expat_process_zip_file(handle->zip, getrelscallbackdata.sharedstringsfile, shared_strings_callback_find_sharedstringtable_start, NULL, NULL, &sharedstringsdata, &sharedstringsdata.xmlparser) != 0) {
    //no shared strings found
    free(sharedstringsdata.text);
    sharedstringlist_destroy(sharedstrings);
    sharedstrings = NULL;
  }
  shared_strings_callback_data_cleanup(&sharedstringsdata);

  //process sheet
  if (!(flags & XLSXIOREAD_NO_CALLBACK)) {
    //use callback mechanism
    struct data_sheet_callback_data processcallbackdata;
    data_sheet_callback_data_initialize(&processcallbackdata, sharedstrings, flags, cell_callback, row_callback, callbackdata);
    expat_process_zip_file(handle->zip, getrelscallbackdata.sheetfile, data_sheet_expat_callback_find_worksheet_start, NULL, NULL, &processcallbackdata, &processcallbackdata.xmlparser);
    data_sheet_callback_data_cleanup(&processcallbackdata);
  } else {
    //use simplified interface by suspending the XML parser when data is found
    xlsxioreadersheet sheethandle = (xlsxioreadersheet)callbackdata;
    data_sheet_callback_data_initialize(&sheethandle->processcallbackdata, sharedstrings, flags, NULL, NULL, sheethandle);
    if ((sheethandle->zipfile = zip_fopen(sheethandle->handle->zip, getrelscallbackdata.sheetfile)) == NULL) {
      result = 1;
    }
    if ((sheethandle->processcallbackdata.xmlparser = expat_process_zip_file_suspendable(sheethandle->zipfile, data_sheet_expat_callback_find_worksheet_start, NULL, NULL, &sheethandle->processcallbackdata)) == NULL) {
      result = 2;
    }
  }

  //clean up
  free(getrelscallbackdata.basepath);
  free(getrelscallbackdata.sheetrelid);
  free(getrelscallbackdata.sheetfile);
  free(getrelscallbackdata.sharedstringsfile);
  return result;
}

////////////////////////////////////////////////////////////////////////

struct xlsxio_read_sheetlist_struct {
  xlsxioreader handle;
  zip_file_t* zipfile;
  struct main_sheet_list_callback_data sheetcallbackdata;
  XML_Parser xmlparser;
  char* nextsheetname;
};

int xlsxioread_list_sheets_resumable_callback (const char* name, void* callbackdata)
{
  //struct main_sheet_list_callback_data* data = (struct main_sheet_list_callback_data*)callbackdata;
  xlsxioreadersheetlist data = (xlsxioreadersheetlist)callbackdata;
  data->nextsheetname = strdup(name);
  XML_StopParser(data->xmlparser, XML_TRUE);
  return 0;
}

void xlsxioread_find_main_sheet_file_callback (zip_t* zip, const char* filename, const char* contenttype, void* callbackdata)
{
  char** data = (char**)callbackdata;
  *data = strdup(filename);
}

DLL_EXPORT_XLSXIO xlsxioreadersheetlist xlsxioread_sheetlist_open (xlsxioreader handle)
{
  //determine main sheet name
  char* mainsheetfile = NULL;
  list_files_by_contenttype(handle->zip, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml", xlsxioread_find_main_sheet_file_callback, &mainsheetfile, NULL);
  if (!mainsheetfile)
    return NULL;
  //process contents of main sheet
  xlsxioreadersheetlist result;
  if ((result = (xlsxioreadersheetlist)malloc(sizeof(struct xlsxio_read_sheetlist_struct))) == NULL)
    return NULL;
  result->handle = handle;
  result->sheetcallbackdata.xmlparser = NULL;
  result->sheetcallbackdata.callback = xlsxioread_list_sheets_resumable_callback;
  result->sheetcallbackdata.callbackdata = result;
  result->nextsheetname = NULL;
  if ((result->zipfile = zip_fopen(handle->zip, mainsheetfile)) != NULL) {
    result->xmlparser = expat_process_zip_file_suspendable(result->zipfile, main_sheet_list_expat_callback_element_start, NULL, NULL, &result->sheetcallbackdata);
  }
  //clean up
  free(mainsheetfile);
  return result;
}

DLL_EXPORT_XLSXIO void xlsxioread_sheetlist_close (xlsxioreadersheetlist sheetlisthandle)
{
  if (!sheetlisthandle)
    return;
  if (sheetlisthandle->xmlparser)
    XML_ParserFree(sheetlisthandle->xmlparser);
  if (sheetlisthandle->zipfile)
    zip_fclose(sheetlisthandle->zipfile);
  free(sheetlisthandle->nextsheetname);
  free(sheetlisthandle);

}

DLL_EXPORT_XLSXIO const char* xlsxioread_sheetlist_next (xlsxioreadersheetlist sheetlisthandle)
{
  if (!sheetlisthandle->zipfile || !sheetlisthandle->xmlparser)
    return NULL;
  free(sheetlisthandle->nextsheetname);
  sheetlisthandle->nextsheetname = NULL;
  enum XML_Status status;
  if ((status = expat_process_zip_file_resume(sheetlisthandle->zipfile, sheetlisthandle->xmlparser)) != XML_STATUS_SUSPENDED) {
    return NULL;
  }
  return sheetlisthandle->nextsheetname;
}

////////////////////////////////////////////////////////////////////////

DLL_EXPORT_XLSXIO xlsxioreadersheet xlsxioread_sheet_open (xlsxioreader handle, const char* sheetname, unsigned int flags)
{
  xlsxioreadersheet result;
  if ((result = (xlsxioreadersheet)malloc(sizeof(struct xlsxio_read_sheet_struct))) == NULL)
    return NULL;
  result->handle = handle;
  result->zipfile = NULL;
  result->lastrownr = 0;
  result->paddingrow = 0;
  result->lastcolnr = 0;
  result->paddingcol = 0;
  xlsxioread_process(handle, sheetname, flags | XLSXIOREAD_NO_CALLBACK, NULL, NULL, result);
  return result;
}

DLL_EXPORT_XLSXIO void xlsxioread_sheet_close (xlsxioreadersheet sheethandle)
{
  if (!sheethandle)
    return;
  if (sheethandle->processcallbackdata.xmlparser)
    XML_ParserFree(sheethandle->processcallbackdata.xmlparser);
  data_sheet_callback_data_cleanup(&sheethandle->processcallbackdata);
  if (sheethandle->zipfile)
    zip_fclose(sheethandle->zipfile);
  free(sheethandle);
}

DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_row (xlsxioreadersheet sheethandle)
{
  enum XML_Status status;
  if (!sheethandle)
    return 0;
  sheethandle->lastcolnr = 0;
  //when padding rows don't retrieve new data
  if (sheethandle->paddingrow) {
    if (sheethandle->paddingrow < sheethandle->processcallbackdata.rownr) {
      return 3;
    } else {
      sheethandle->paddingrow = 0;
      return 2;
    }
  }
  sheethandle->paddingcol = 0;
  //go to beginning of next row
  while ((status = expat_process_zip_file_resume(sheethandle->zipfile, sheethandle->processcallbackdata.xmlparser)) == XML_STATUS_SUSPENDED && sheethandle->processcallbackdata.colnr != 0) {
  }
  return (status == XML_STATUS_SUSPENDED ? 1 : 0);
}

DLL_EXPORT_XLSXIO char* xlsxioread_sheet_next_cell (xlsxioreadersheet sheethandle)
{
  char* result;
  if (!sheethandle)
    return NULL;
  //append empty column if needed
  if (sheethandle->paddingcol) {
    if (sheethandle->paddingcol > sheethandle->processcallbackdata.cols) {
      //last empty column added, finish row
      sheethandle->paddingcol = 0;
      //when padding rows prepare for the next one
      if (sheethandle->paddingrow) {
        sheethandle->lastrownr++;
        sheethandle->paddingrow++;
        if (sheethandle->paddingrow + 1 < sheethandle->processcallbackdata.rownr) {
          sheethandle->paddingcol = 1;
        }
      }
      return NULL;
    } else {
      //add another empty column
      sheethandle->paddingcol++;
      return strdup("");
    }
  }
  //get value
  if (!sheethandle->processcallbackdata.celldata)
    if (expat_process_zip_file_resume(sheethandle->zipfile, sheethandle->processcallbackdata.xmlparser) != XML_STATUS_SUSPENDED)
      sheethandle->processcallbackdata.celldata = NULL;
  //insert empty rows if needed
  if (!(sheethandle->processcallbackdata.flags & XLSXIOREAD_SKIP_EMPTY_ROWS) && sheethandle->lastrownr + 1 < sheethandle->processcallbackdata.rownr) {
    sheethandle->paddingrow = sheethandle->lastrownr + 1;
    sheethandle->paddingcol = sheethandle->processcallbackdata.colnr*0 + 1;
    return xlsxioread_sheet_next_cell(sheethandle);
  }
  //insert empty column before if needed
  if (!(sheethandle->processcallbackdata.flags & XLSXIOREAD_SKIP_EMPTY_CELLS)) {
    if (sheethandle->lastcolnr + 1 < sheethandle->processcallbackdata.colnr) {
      sheethandle->lastcolnr++;
      return strdup("");
    }
  }
  result = sheethandle->processcallbackdata.celldata;
  sheethandle->processcallbackdata.celldata = NULL;
  //end of row
  if (!result) {
    sheethandle->lastrownr = sheethandle->processcallbackdata.rownr;
    //insert empty column at end if row if needed
    if (!result && !(sheethandle->processcallbackdata.flags & XLSXIOREAD_SKIP_EMPTY_CELLS) && sheethandle->processcallbackdata.colnr < sheethandle->processcallbackdata.cols) {
      sheethandle->paddingcol = sheethandle->lastcolnr + 1;
      return xlsxioread_sheet_next_cell(sheethandle);
    }
  }
  sheethandle->lastcolnr = sheethandle->processcallbackdata.colnr;
  return result;
}

DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_string (xlsxioreadersheet sheethandle, char** pvalue)
{
  char* result;
  if (!sheethandle)
    return -1;
  result = xlsxioread_sheet_next_cell(sheethandle);
  if (pvalue)
    *pvalue = result;
  return (result ? 1 : 0);
}

DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_int (xlsxioreadersheet sheethandle, int64_t* pvalue)
{
  char* result;
  int status;
  if ((result = xlsxioread_sheet_next_cell(sheethandle)) != NULL) {
    if (pvalue) {
      status = sscanf(result, "%" PRIi64, pvalue);
      if (status == EOF || status == 0)
        *pvalue = 0;
      //alternative: use strtoimax()
    }
  }
  return (result ? 1 : 0);
}

DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_float (xlsxioreadersheet sheethandle, double* pvalue)
{
  char* result;
  if ((result = xlsxioread_sheet_next_cell(sheethandle)) != NULL) {
    if (pvalue)
      *pvalue = strtod(result, NULL);
  }
  return (result ? 1 : 0);
}

DLL_EXPORT_XLSXIO int xlsxioread_sheet_next_cell_datetime (xlsxioreadersheet sheethandle, time_t* pvalue)
{
  char* result;
  if ((result = xlsxioread_sheet_next_cell(sheethandle)) != NULL) {
    if (pvalue) {
      double value = strtod(result, NULL);
      if (value != 0) {
        value = (value - 25569) * 86400;  //converstion from Excel to Unix timestamp
      }
      *pvalue = time_t(value);
    }
  }
  return (result ? 1 : 0);
}

