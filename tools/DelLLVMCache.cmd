@echo off
if not exist %localappdata%\zdoom\cache\llvm* goto :eof
echo QZDoom's LLVM drawers may take some time to create at startup. Because of this,
echo the program uses a cache to temporarily store bitcode for faster startups. If
echo this cache is ever corrupted, this program has been created to solve the
echo problem.
echo.
echo Are you SURE you wish to destroy the cache?
del /p %localappdata%\zdoom\cache\llvm*
