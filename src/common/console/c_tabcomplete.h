#pragma once

void C_AddTabCommand (const char *name);
void C_RemoveTabCommand (const char *name);
void C_ClearTabCommands();		// Removes all tab commands
void C_TabComplete(bool goForward);
bool C_TabCompleteList();
extern bool TabbedLast;		// True if last key pressed was tab
extern bool TabbedList;		// True if tab list was shown
