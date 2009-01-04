#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <sys/ucontext.h>

static const char *cc_logfile = NULL;

static char respfile[256];
static char buf[256];
static char user_info_buf[1024];

static const struct {
	const char *name;
	int signum;
} signals[] = {
	{ "Segmentation fault", SIGSEGV },
	{ "Illegal instruction", SIGILL },
	{ "FPU exception", SIGFPE },
	{ "System BUS error", SIGBUS },
	{ NULL, 0 }
};

static const struct {
	int code;
	const char *name;
} sigill_codes[] = {
#ifndef __FreeBSD__
	{ ILL_ILLOPC, "Illegal opcode" },
	{ ILL_ILLOPN, "Illegal operand" },
	{ ILL_ILLADR, "Illegal addressing mode" },
	{ ILL_ILLTRP, "Illegal trap" },
	{ ILL_PRVOPC, "Privileged opcode" },
	{ ILL_PRVREG, "Privileged register" },
	{ ILL_COPROC, "Coprocessor error" },
	{ ILL_BADSTK, "Internal stack error" },
#endif
	{ 0, NULL }
};

static const struct {
	int code;
	const char *name;
} sigfpe_codes[] = {
	{ FPE_INTDIV, "Integer divide by zero" },
	{ FPE_INTOVF, "Integer overflow" },
	{ FPE_FLTDIV, "Floating point divide by zero" },
	{ FPE_FLTOVF, "Floating point overflow" },
	{ FPE_FLTUND, "Floating point underflow" },
	{ FPE_FLTRES, "Floating point inexact result" },
	{ FPE_FLTINV, "Floating point invalid operation" },
	{ FPE_FLTSUB, "Subscript out of range" },
	{ 0, NULL }
};

static const struct {
	int code;
	const char *name;
} sigsegv_codes[] = {
#ifndef __FreeBSD__
	{ SEGV_MAPERR, "Address not mapped to object" },
	{ SEGV_ACCERR, "Invalid permissions for mapped object" },
#endif
	{ 0, NULL }
};

static const struct {
	int code;
	const char *name;
} sigbus_codes[] = {
#ifndef __FreeBSD__
	{ BUS_ADRALN, "Invalid address alignment" },
	{ BUS_ADRERR, "Non-existent physical address" },
	{ BUS_OBJERR, "Object specific hardware error" },
#endif
	{ 0, NULL }
};

static int (*cc_user_info)(char*, char*);

static void gdb_info(pid_t pid)
{
	FILE *f;
	int fd;

	/* Create a temp file to put gdb commands into */
	strcpy(respfile, "gdb-respfile-XXXXXX");
	if((fd = mkstemp(respfile)) >= 0 && (f = fdopen(fd, "w")))
	{
		fprintf(f, "signal SIGCHLD\n"
		           "shell echo \"\"\n"
		           "shell echo \"* Loaded Libraries\"\n"
		           "info sharedlibrary\n"
		           "shell echo \"\"\n"
		           "shell echo \"* Threads\"\n"
		           "info threads\n"
		           "shell echo \"\"\n"
		           "shell echo \"* FPU Status\"\n"
		           "info float\n"
		           "shell echo \"\"\n"
		           "shell echo \"* Registers\"\n"
		           "info registers\n"
		           "shell echo \"\"\n"
		           "shell echo \"* Bytes near %%eip:\"\n"
		           "x/x $eip-3\nx/x $eip\n"
		           "shell echo \"\"\n"
		           "shell echo \"* Backtrace\"\n"
		           "backtrace full\n"
#if 0 /* This sorta works to print out the core, but is too slow and skips 0's.. */
		           "shell echo \"\"\n"
		           "shell echo \"* Stack\"\n"
		           "set var $_sp = $esp\n"
		           "while $_sp <= $ebp - 12\n"
		           " printf \"%%08x: \", $_sp\n"
		           "  set var $_i = $_sp\n"
		           "  while $_i < $_sp + 16\n"
		           "    printf \"%%08x \", {int} $_i\n"
		           "    set $_i += 4\n"
		           "  end\n"
		           "  set var $_i = $_sp\n"
		           "  while $_i < $_sp + 16\n"
		           "    printf \"%%c\", {int} $_i\n"
		           "    set ++$_i\n"
		           "  end\n"
		           "  set var $_sp += 16\n"
		           "  printf \"\\n\"\n"
		           "end\n"
		           "if $_sp <= $ebp\n"
		           "  printf \"%%08x: \", $esp\n"
		           "  while $_sp <= $ebp\n"
		           "    printf \"%%08x \", {int} $_i\n"
		           "    set $_sp += 4\n"
		           "  end\n"
		           "  printf \"\\n\"\n"
		           "end\n"
#endif
		           "kill\n"
		           "quit\n");
		fclose(f);

		/* Run gdb and print process info. */
		snprintf(buf, sizeof(buf), "gdb --quiet --batch --command=%s --pid=%i", respfile, pid);
		printf("Executing: %s\n", buf);
		fflush(stdout);
		system(buf);

		/* Clean up */
		remove(respfile);
	}
	else
	{
		/* Error creating temp file */
		if(fd >= 0)
		{
			close(fd);
			remove(respfile);
		}
		printf("Could not create gdb command file\n");
	}
	fflush(stdout);
}


/* Generic system info */
static void sys_info(void)
{
#if (defined __unix__)
	system("echo \"System: `uname -a`\"");
#endif
	system("echo \"GCC version: `gcc -dumpversion`\"");
	putchar('\n');
	fflush(stdout);
}

static void crash_catcher(int signum, siginfo_t *siginfo, void *context)
{
	ucontext_t *ucontext = (ucontext_t*)context;
	const char *sigdesc = NULL;
	pid_t pid, dbg_pid;
	struct stat sbuf;
#ifndef __FreeBSD__
	struct rlimit rl;
#endif
	int status, i;
	FILE *f;

	/* Get the signal description */
	if(!siginfo)
	{
		for(i = 0;signals[i].name;++i)
		{
			if(signals[i].signum == signum)
			{	
				sigdesc = signals[i].name;
				break;
			}
		}
	}
	else
	{
		switch(signum)
		{
			case SIGSEGV:
				for(i = 0;sigsegv_codes[i].name;++i)
				{
					if(sigsegv_codes[i].code == siginfo->si_code)
					{
						sigdesc = sigsegv_codes[i].name;
						break;
					}
				}
				break;

			case SIGFPE:
				for(i = 0;sigfpe_codes[i].name;++i)
				{
					if(sigfpe_codes[i].code == siginfo->si_code)
					{
						sigdesc = sigfpe_codes[i].name;
						break;
					}
				}
				break;

			case SIGILL:
				for(i = 0;sigill_codes[i].name;++i)
				{
					if(sigill_codes[i].code == siginfo->si_code)
					{
						sigdesc = sigill_codes[i].name;
						break;
					}
				}
				break;

			case SIGBUS:
				for(i = 0;sigbus_codes[i].name;++i)
				{
					if(sigbus_codes[i].code == siginfo->si_code)
					{
						sigdesc = sigbus_codes[i].name;
						break;
					}
				}
				break;
		}
	}

	if(!sigdesc)
	{
		/* Unknown signal, let the default handler deal with it */
		raise(signum);
		return;
	}

#if 0 /* Do we need this? */
	/* Make sure the effective uid is the real uid */
	if (getuid() != geteuid())
	{
		fprintf(stderr, "%s (signal %i)\ngetuid() does not match geteuid().\n", sigdesc, signum);
		_exit(-1);
	}
#endif

	/* Create crash log file */
	if(cc_logfile)
	{
		f = fopen(cc_logfile, "w");
		if(!f)
		{
			fprintf(stderr, "Could not create %s following signal.\n", cc_logfile);
			raise(signum);
			return;
		}
	}
	else
		f = stderr;

	/* Get current process id and fork off */
	pid = getpid();
	switch ((dbg_pid = fork()))
	{
		/* Error */
		case -1:
			break;

		case 0:
			fprintf(stderr, "\n\n*** Fatal Error ***\n"
			                "%s (signal %i)\n", sigdesc, signum);

			/* Redirect shell output */
			close(STDOUT_FILENO);
			dup2(fileno(f), STDOUT_FILENO);

			if(f != stderr)
			{
				fprintf(stderr, "\nGenerating %s and killing process %i, please wait... ", cc_logfile, pid);
				fprintf(f, "*** Fatal Error ***\n"
				           "%s (signal %i)\n", sigdesc, signum);
			}
			if(siginfo)
				fprintf(f, "Address: %p\n", siginfo->si_addr);
			fputc('\n', f);
			fflush(f);

			/* Get info */
			sys_info();
			if(cc_user_info)
			{
				if(cc_user_info(user_info_buf, user_info_buf+sizeof(user_info_buf)) > 0)
				{
					fprintf(f, "%s\n", user_info_buf);
					fflush(f);
				}
			}
			gdb_info(pid);

#if 0 /* Why won't this work? */
			if(ucontext)
			{
				unsigned char *ptr = ucontext->uc_stack.ss_sp;
				size_t len;

				fprintf(f, "\n* Stack\n");
				for(len = ucontext->uc_stack.ss_size/4;len > 0; len -= 4)
				{
					fprintf(f, "0x%08x:", (int)ptr);
					for(i = 0;i < ((len < 4) ? len : 4);++i)
					{
						fprintf(f, " %02x%02x%02x%02x", ptr[i*4 + 0], ptr[i*4 + 1],
						                                ptr[i*4 + 2], ptr[i*4 + 3]);
					}
					fputc(' ', f);
					fflush(f);
					for(i = 0;i < ((len < 4) ? len : 4);++i)
					{
						fprintf(f, "%c", *(ptr++));
						fprintf(f, "%c", *(ptr++));
						fprintf(f, "%c", *(ptr++));
						fprintf(f, "%c", *(ptr++));
					}
					fputc('\n', f);
					fflush(f);
				}
			}
#endif

			if(f != stderr)
			{
				fclose(f);
#if (defined __unix__)
				if(cc_logfile)
				{
					char buf[256];
					snprintf(buf, sizeof(buf),
					         "if (which gxmessage > /dev/null 2>&1);"
					             "then gxmessage -buttons \"Damn it:0\" -center -title \"Very Fatal Error\" -file %s;"
					         "elif (which xmessage > /dev/null 2>&1);"
					             "then xmessage -buttons \"Damn it:0\" -center -file %s -geometry 600x400;"
					         "fi", cc_logfile, cc_logfile);
					system(buf);
				}
#endif
			}

			_exit(0);

		default:
			/* Wait and let the child attach gdb */
			waitpid(dbg_pid, NULL, 0);
	}
}

int cc_install_handlers(int num_signals, int *signals, const char *logfile, int (*user_info)(char*, char*))
{
	int retval = 0;
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));

	sa.sa_sigaction = crash_catcher;
	
#ifndef __FreeBSD__
	sa.sa_flags = SA_ONESHOT | SA_NODEFER | SA_SIGINFO;
#else
	sa.sa_flags = SA_NODEFER | SA_SIGINFO;
#endif

	cc_logfile = logfile;
	cc_user_info = user_info;

	while(num_signals--)
	{
		if((*signals != SIGSEGV && *signals != SIGILL && *signals != SIGFPE &&
		    *signals != SIGBUS) || sigaction(*signals, &sa, NULL) == -1)
		{
			*signals = 0;
			retval = -1;
		}
		++signals;
	}

	return retval;
}
