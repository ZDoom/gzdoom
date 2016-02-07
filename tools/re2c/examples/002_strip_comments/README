re2c lesson 002_strip_comments, (c) M. Boerger 2006

In this lesson you will learn how to use multiple scanner blocks and how to 
read the input from a file instead of a zero terminated string. In the end you
will have a scanner that filters comments out of c source files but keeps re2c
comments.

The first scanner can be generated with:

  re2c -s -o t.c strip_001.s.re

In the second step we will learn about YYMARKER that stores backtracking 
information.

  re2c -s -0 t.c strip_002.b.re

The third step brings trailing contexts that are stored in YYCTXMARKER. We also
change to use -b instead of -s option since the scanner gets more and more
complex.

  re2c -b -0 t.c strip_002.b.re
