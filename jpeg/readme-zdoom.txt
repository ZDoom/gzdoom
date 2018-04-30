This is a stripped version of the Independant JPEG Group's library,
available at <http://www.ijg.org/>. The following features have been
removed to decrease source code size:

* All encoding code.
* All sample applications.
* Most documentation.
* Unix configure scripts.
* Multiple Makefiles.
* Multiple memory managers.
* Disk-based backing store. If you don't have enough memory to decode
  a JPEG file, you probably can't play ZDoom either.
* Far pointers. Who cares about 16-bit x86? Not me.
* Transcoding routines.
* Buffered image output.
* Raw data output.

A Unix build of ZDoom should just use the system libjpeg instead of
this code.
