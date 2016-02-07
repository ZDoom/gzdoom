re2c lesson 001_upn_calculator, (c) M. Boerger 2006

This lesson gets you started with re2c. In the end you will have an easy RPN
(reverse polish notation) calculator for use at command line.

You will learn about the basic interface of re2c when scanning input strings. 
How to detect the end of the input and use that to stop scanning in order to
avoid problems.

Once you have successfully installed re2c you can use it to generate *.c files
from the *.re files presented in this lesson. Actually the expected *.c files 
are already present. So you should name them *.cc or something alike or just 
give them a different name like test.c. To do so you simply change into the 
directory and execute the following command:

  re2c calc_001.re > test.c

Then use your compiler to compile that code and run it. If you are using gcc
you simply do the following:

  gcc -o test.o test.c
  ./test.o <input_file_name>

If you are using windows you might want to read till the end of this lesson.

When you want to debug the code it helps to make re2c generate working #line
information. To do so you simply specify the output file using the -o switch 
followed by the output filename:

  re2c -o test.c calc_001.re

The input files *.re each contain basic step by comments that explain what is
going on and what you can see in the examples.

In order to optimize the generated code we will use the -s command line switch
of re2c. This tells re2c to generate code that uses if statements rather 
then endless switch/case expressions where appropriate. Note that the file name
extension is actually '.s.re' to tell the test system to use the -s switch. To
invoke re2 you do the following:

  re2c -s -o test.c calc_006.s.re

Finally we use the -b switch to have the code use a decision table. The -b 
switch also contains the -s behavior. 

  re2c -b -o test.c calc_007.b.re



-------------------------------------------------------------------------------

For windows users Lynn Allan provided some additional stuff to get you started 
in the Microsoft world. This addon resides in the windows subdirectory and 
gives you something to expereiment with. The code in that directory is based 
on the first step and has the following changes:

* vc6 .dsp/.dsw and vc7/vc8 .sln/.vcproj project files that have "Custom Build 
Steps" that can tell when main.re changes, and know how to generate main.c 
from main.re. They assume that you unpacked the zip package and have re2c
itself build or installed in Release and Release-2005 directory respectively.
If re2c cannot be found you need to modify the custom build step and correct
the path to re2c.

* BuildAndRun.bat to do command line rec2 and then cl and then run the 
executable (discontinues with message if errors).

* built-in cppunit-like test to confirm it worked as expected.

* array of test strings "fed" to scan rather than file contents to facilitate 
testing and also reduce the newbie learning curve.

* HiResTimer output for 10,000 loops and 100,000 loops. While this might be 
excessive for this lesson, it illustrates how to do it for subsequent lessons
and your own stuff using windows. Also it shows that Release build is as fast 
as strncmp for this test and can probably be made significantly faster.

* If you want to build the other steps of this lesson using windows tools 
simply copy the *.re files into the windows directory as main.re and rebuild.


-------------------------------------------------------------------------------
Sidenote: UPN is the german translation of RPN, somehow hardcoded into the 
authors brain :-)
