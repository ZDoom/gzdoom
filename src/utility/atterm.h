#pragma once

void addterm (void (*func)(void), const char *name);
#define atterm(t) addterm (t, #t)
void popterm ();
void call_terms();
