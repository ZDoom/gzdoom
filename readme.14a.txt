This archive contains a unified diff that will patch the source code for
ZDoom version 1.14 to 1.14a. To use it, you need to have already downloaded
the archive zdoom114-src.zip and extracted its contains. Then extract
patch.diff from this archive into your source code directory and apply it
with the command:

    patch -F 3 -i patch.diff

(You need to have kept the patch.exe included with zdoom114-src.zip.)

Note that there was another version of this archive from July. The only
difference between that one and this one is that this one fixes the
savegame loading bug. If you already installed that patch, you can do the
fix yourself by changing one string in G_DoLoadGame() in g_game.c. The
string "version %i" should be changed to "version %ia".

Randy Heit
rheit@usa.net