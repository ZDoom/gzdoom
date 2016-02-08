/* re2c lesson 001_upn_calculator, main.b.re, (c) M. Boerger, L. Allan 2006 */
/*!ignore:re2c

- basic interface for string reading

  . We define the macros YYCTYPE, YYCURSOR, YYLIMIT, YYMARKER, YYFILL
  . YYCTYPE is the type re2c operates on or in other words the type that 
    it generates code for. While it is not a big difference when we were
    using 'unsigned char' here we would need to run re2c with option -w
    to fully support types with sieof() > 1.
  . YYCURSOR is used internally and holds the current scanner position. In
    expression handlers, the code blocks after re2c expressions, this can be 
    used to identify the end of the token.
  . YYMARKER is not always being used so we set an initial value to avoid
    a compiler warning.
  . YYLIMIT stores the end of the input. Unfortunatley we have to use strlen() 
    in this lesson. In the next example we see one way to get rid of it.
  . We use a 'for(;;)'-loop around the scanner block. We could have used a
    'while(1)'-loop instead but some compilers generate a warning for it.
  . To make the output more readable we use 're2c:indent:top' scanner 
    configuration that configures re2c to prepend a single tab (the default)
    to the beginning of each output line.
  . The following lines are expressions and for each expression we output the 
    token name and continue the scanner loop.
  . The second last token detects the end of our input, the terminating zero in
    our input string. In other scanners detecting the end of input may vary.
    For example binary code may contain \0 as valid input.
  . The last expression accepts any input character. It tells re2c to accept 
    the opposit of the empty range. This includes numbers and our tokens but
    as re2c goes from top to botton when evaluating the expressions this is no 
    problem.
  . The first three rules show that re2c actually prioritizes the expressions 
    from top to bottom. Octal number require a starting "0" and the actual 
    number. Normal numbers start with a digit greater 0. And zero is finally a
    special case. A single "0" is detected by the last rule of this set. And
    valid ocal number is already being detected by the first rule. This even
    includes multi "0" sequences that in octal notation also means zero.
    Another way would be to only use two rules:
    "0" [0-9]+
    "0" | ( [1-9] [0-9]* )
    A full description of re2c rule syntax can be found in the manual.
*/

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#if _MSC_VER > 1200
#define WINVER 0x0400   // Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif                  // Prevents warning from vc7.1 complaining about redefinition

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <windows.h>
#include "HiResTimer.h"

static char gTestBuf[1000] = "";

/**
 * @brief Setup HiResolution timer and confirm it is working ok
 */
void InitHiResTimerAndVerifyWorking(void)
{
   double elapsed;
   HrtInit();
   HrtSetPriority(ABOVE_NORMAL_PRIORITY_CLASS);
   HrtStart();
   Sleep(100);
   elapsed = HrtElapsedMillis();
   if ((elapsed < 90) || (elapsed > 110)) {
      printf("HiResTimer misbehaving: %f\n", elapsed);
      exit(2);
   }
}

/**
 * @brief Scan for numbers in different formats
 */
int ScanFullSpeed(char *pzStrToScan, size_t lenStrToScan)
{
	unsigned char *pzCurScanPos = (unsigned char*)pzStrToScan;
	unsigned char *pzBacktrackInfo = 0;
#define YYCTYPE         unsigned char
#define YYCURSOR        pzCurScanPos
#define YYLIMIT         (pzStrToScan+lenStrToScan)
#define YYMARKER        pzBacktrackInfo
#define YYFILL(n)
	
	for(;;)
	{
/*!re2c
	re2c:indent:top = 2;
	[1-9][0-9]*	{ continue; }
	[0][0-9]+	{ continue; }
	"+"			{ continue; }
	"-"			{ continue; }
	"\000"		{ return 0; }
	[^]			{ return 1; }
*/
	}
}

/**
 * @brief Scan for numbers in different formats
 */
int scan(char *pzStrToScan, size_t lenStrToScan)
{
	unsigned char *pzCurScanPos = (unsigned char*)pzStrToScan;
	unsigned char *pzBacktrackInfo = 0;
#define YYCTYPE         unsigned char
#define YYCURSOR        pzCurScanPos
#define YYLIMIT         (pzStrToScan+lenStrToScan)
#define YYMARKER        pzBacktrackInfo
#define YYFILL(n)
	
	for(;;)
	{
/*!re2c
	re2c:indent:top = 2;
	[1-9][0-9]*	{ printf("Num\n"); strcat(gTestBuf, "Num "); continue; }
	[0][0-9]+	{ printf("Oct\n"); strcat(gTestBuf, "Oct "); continue; }
	"+"			{ printf("+\n");   strcat(gTestBuf, "+ ");   continue; }
	"-"			{ printf("-\n");   strcat(gTestBuf, "- ");   continue; }
	"\000"		{ printf("EOF\n");                           return 0; }
	[^]			{ printf("ERR\n"); strcat(gTestBuf, "ERR "); return 1; }
*/
	}
}

/**
 * @brief Show high resolution elapsed time for 10,000 and 100,000 loops
 */
void DoTimingsOfStrnCmp(void)
{
   char testStr[] = "Hello, world";
   int totLoops = 10000;
   int totFoundCount = 0;
   int foundCount = 0;
   int loop;
   int rc;
   const int progressAnd = 0xFFFFF000;
   double elapsed;

   printf("\n\n%d loops with * every %d loops to confirm\n", totLoops, ((~progressAnd) + 1));

   HrtStart();
   for (loop = 0; loop < totLoops; ++loop) {
      foundCount = 0;
      rc = strncmp(testStr, "Hello", 5);
      if (rc == 0) {
         foundCount++;
         totFoundCount++;
         if ((totFoundCount & progressAnd) == totFoundCount) {
            printf("*");
         }
      }
   }
   elapsed = HrtElapsedMillis();
   printf("\nstrncmp Elapsed for %7d loops milliseconds: %7.3f\n", totLoops, elapsed);
   printf("FoundCount each loop: %d\n", foundCount);
   printf("TotalFoundCount for all loops: %d\n", totFoundCount);

   totLoops = 100000;
   HrtStart();
   for (loop = 0; loop < totLoops; ++loop) {
      foundCount = 0;
      rc = strncmp(testStr, "Hello", 5);
      if (rc == 0) {
         foundCount++;
         totFoundCount++;
         if ((totFoundCount & progressAnd) == totFoundCount) {
            printf("*");
         }
      }
   }
   elapsed = HrtElapsedMillis();
   printf("\nstrncmp Elapsed for %7d loops milliseconds: %7.3f\n", totLoops, elapsed);
   printf("FoundCount each loop: %d\n", foundCount);
   printf("TotalFoundCount for all loops: %d\n", totFoundCount);
}

/**
 * @brief Show high resolution elapsed time for 10,000 and 100,000 loops
 */
void DoTimingsOfRe2c(void)
{
   char* testStrings[] = { "123", "1234", "+123", "01234", "-04321", "abc", "123abc" };
   const int testCount = sizeof(testStrings) / sizeof(testStrings[0]);
   int i;
   int totLoops = 10000 / testCount;  // Doing more than one per loop
   int totFoundCount = 0;
   int foundCount = 0;
   int loop;
   int rc;
   const int progressAnd = 0xFFFFF000;
   double elapsed;

   printf("\n\n%d loops with * every %d loops to confirm\n", totLoops, ((~progressAnd) + 1));

   HrtStart();
   for (loop = 0; loop < totLoops; ++loop) {
      foundCount = 0;
      strcpy(gTestBuf, "");   
      for (i = 0; i < testCount; ++i) {
         char* pzCurStr = testStrings[i];
         size_t len = strlen(pzCurStr);  // Calc of strlen slows things down ... std::string?
         rc = ScanFullSpeed(pzCurStr, len);
         if (rc == 0) {
            foundCount++;
            totFoundCount++;
            if ((totFoundCount & progressAnd) == totFoundCount) {
               printf("*");
            }
         }
      }
   }
   elapsed = HrtElapsedMillis();
   printf("\nRe2c Elapsed for %7d loops milliseconds: %7.3f\n", totLoops, elapsed);
   printf("FoundCount each loop: %d\n", foundCount);
   printf("TotalFoundCount for all loops: %d\n", totFoundCount);

   totLoops = 100000 / testCount;
   printf("\n\n%d loops with * every %d loops to confirm\n", totLoops, ((~progressAnd) + 1));

   HrtStart();
   for (loop = 0; loop < totLoops; ++loop) {
      foundCount = 0;
      strcpy(gTestBuf, "");   
      for (i = 0; i < testCount; ++i) {
         char* pzCurStr = testStrings[i];
         size_t len = strlen(pzCurStr);  // Calc of strlen slows things down ... std::string?
         rc = ScanFullSpeed(pzCurStr, len);
         if (rc == 0) {
            foundCount++;
            totFoundCount++;
            if ((totFoundCount & progressAnd) == totFoundCount) {
               printf("*");
            }
         }
      }
   }
   elapsed = HrtElapsedMillis();
   printf("\nRe2c Elapsed for %7d loops milliseconds: %7.3f\n", totLoops, elapsed);
   printf("FoundCount each loop: %d\n", foundCount);
   printf("TotalFoundCount for all loops: %d\n", totFoundCount);
}

/**
 * @brief Entry point for console app
 */
int main(int argc, char **argv)
{
   char  testStr_A[] = "123";
   char* testStr_B   = "456";
   char* testStrings[] = { "123", "1234", "+123", "01234", "-04321", "abc", "123abc" };
   const int testCount = sizeof(testStrings) / sizeof(testStrings[0]);
   int i;
   
   int rc = scan(testStr_A, 3);
   printf("rc: %d\n", rc);

   rc = scan(testStr_B, 3);
   printf("rc: %d\n", rc);

   rc = scan("789", 3);
   printf("rc: %d\n", rc);

   strcpy(gTestBuf, "");   
   for (i = 0; i < testCount; ++i) {
      char* pzCurStr = testStrings[i];
      size_t len = strlen(pzCurStr);
      scan(pzCurStr, len);
   }
   printf("%s\n", gTestBuf);
   rc = strcmp(gTestBuf, "Num Num + Num Oct - Oct ERR Num ERR ");
   if (rc == 0) {
      printf("Success\n");
   }
   else {
      printf("Failure\n");
   }
   assert(0 == rc); // Doesn't work with Release build
   
   InitHiResTimerAndVerifyWorking();

   DoTimingsOfStrnCmp();
   
   DoTimingsOfRe2c();
   
   return 0;
}
