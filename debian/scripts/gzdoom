#!/bin/sh

cd /usr/lib/games/gzdoom &&
LD_LIBRARY_PATH="$PWD:$LD_LIBRARY_PATH" ./gzdoom "$@"

if [ $? != 0 ] ; then
    zenity --warning --text \
"Something went wrong!\nIf you haven't installed any Doom IWAD files yet or if GZDoom "\
"is unable to locate them, put the files or symlinks to them in '$HOME/.config/gzdoom' "\
"or start the game with the command 'DOOMWADDIR=/path/to/iwadfiledir gzdoom'."
fi
