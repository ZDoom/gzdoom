#include "v_video.h"

// Start writing an 8-bit palettized PNG file.
// The passed file should be a newly created file.
// This function writes the PNG signature and the IHDR, gAMA, PLTE, and IDAT
// chunks.
bool M_CreatePNG (FILE *file, const DCanvas *canvas, const PalEntry *pal);

// Creates a grayscale 1x1 PNG file. Used for savegames without savepics.
bool M_CreateDummyPNG (FILE *file);

// Appends any chunk to a PNG file started with M_CreatePNG.
bool M_AppendPNGChunk (FILE *file, DWORD chunkID, const BYTE *chunkData, DWORD len);

// Adds a tEXt chunk to a PNG file started with M_CreatePNG.
bool M_AppendPNGText (FILE *file, const char *keyword, const char *text);

// Appends the IEND chunk to a PNG file.
bool M_FinishPNG (FILE *file);

// Finds a chunk in a PNG file. The file pointer will be positioned at the
// beginning of the chunk data, and its length will be returned. A return
// value of 0 indicates the chunk was either not present or had 0 length.
size_t M_FindPNGChunk (FILE *file, DWORD chunkID);

// Finds a chunk in the PNG file, starting its search at whatever chunk
// the file pointer is currently positioned at.
size_t M_NextPNGChunk (FILE *file, DWORD chunkID);

// Find a PNG text chunk with the given signature. The file will be positioned
// at the start of the text (after the keyword), and the length of the text
// is returned. This only support tEXt, not zTXt.
size_t M_FindPNGText (FILE *file, const char *keyword);

// Finds a PNG text chunk with the given signature and returns a pointer
// to a NULL-terminated string if present. Returns NULL on failure.
char *M_GetPNGText (FILE *file, const char *keyword);

// Verify that a file really is a PNG file. This includes not only checking
// the signature, but also checking for the IEND chunk. CRC checking of
// each chunk is not done.
bool M_VerifyPNG (FILE *file);

// Creates a simple canvas containing the contents of the PNG file's IDAT
// chunk(s). Only 8-bit images are supported.
DCanvas *M_CreateCanvasFromPNG (FILE *file);
