// GCC does not support Windows' Structured Exception Handling. :-(
// So I have to fake it using inline assembly

#ifdef __GNUC__

#define __try try
#define __except(a)	catch(...)

#define EH_NONCONTINUABLE       0x01
#define EH_UNWINDING            0x02
#define EH_EXIT_UNWIND          0x04
#define EH_STACK_INVALID        0x08
#define EH_NESTED_CALL          0x10

typedef enum {
	ExceptionContinueExecution,
	ExceptionContinueSearch,
	ExceptionNestedException,
	ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;

typedef EXCEPTION_DISPOSITION (*PEXCEPTION_HANDLER)
            (struct _EXCEPTION_RECORD*, void*, struct _CONTEXT*, void*);

typedef struct _EXCEPTION_REGISTRATION
{
	struct _EXCEPTION_REGISTRATION* Prev;
	PEXCEPTION_HANDLER              Handler;
} EXCEPTION_REGISTRATION, *PEXCEPTION_REGISTRATION;

#endif
