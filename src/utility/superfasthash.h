#pragma once

extern unsigned int MakeKey (const char *s);
extern unsigned int MakeKey (const char *s, size_t len);
extern unsigned int SuperFastHash (const char *data, size_t len);
