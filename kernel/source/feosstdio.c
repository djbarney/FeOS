#include "feos.h"
#include "fxe.h"
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE* stdin_hook = NULL;
FILE* stdout_hook = NULL;
FILE* stderr_hook = NULL;

FILE* FeOS_GetStdin()
{
	return !stdin_hook ? stdin : stdin_hook;
}

FILE* FeOS_GetStdout()
{
	return !stdout_hook ? stdout : stdout_hook;
}

FILE* FeOS_GetStderr()
{
	return !stderr_hook ? stderr : stderr_hook;
}

FILE* FeOS_SetStdin(FILE* newstdin)
{
	FILE* oldstdin = stdin_hook;
	stdin_hook = newstdin;
	return oldstdin;
}

FILE* FeOS_SetStdout(FILE* newstdout)
{
	FILE* oldstdout = stdout_hook;
	stdout_hook = newstdout;
	return oldstdout;
}

FILE* FeOS_SetStderr(FILE* newstderr)
{
	FILE* oldstderr = stderr_hook;
	stderr_hook = newstderr;
	return oldstderr;
}

FILE* FeOS_OpenStream(const void*);

BEGIN_TABLE(FEOSSTDIO)
	ADD_FUNC(FeOS_GetStdin),
	ADD_FUNC(FeOS_GetStdout),
	ADD_FUNC(FeOS_GetStderr),
	ADD_FUNC(FeOS_SetStdin),
	ADD_FUNC(FeOS_SetStdout),
	ADD_FUNC(FeOS_SetStderr),
	ADD_FUNC(FeOS_OpenStream),

	// Basic I/O
	ADD_FUNC(fopen),
	ADD_FUNC(freopen),
	ADD_FUNC(fclose),
	ADD_FUNC(fwrite),
	ADD_FUNC(fread),
	ADD_FUNC(feof),
	ADD_FUNC(fseek),
	ADD_FUNC(ftell),
	ADD_FUNC(fflush),
	ADD_FUNC(ferror),
	ADD_FUNC(clearerr),

	// Formatting
	ADD_FUNC_ALIAS(vfiprintf, vfprintf),
	ADD_FUNC_ALIAS(vsiprintf, vsprintf),
	ADD_FUNC_ALIAS(vfiscanf, vfscanf),
	ADD_FUNC_ALIAS(vsiscanf, vsscanf),

	// Strings and chars
	ADD_FUNC(fgetc), ADD_FUNC(fputc),
	ADD_FUNC(fgets), ADD_FUNC(fputs),
	ADD_FUNC(ungetc),

	// Misc
	ADD_FUNC(remove),
	ADD_FUNC(rename)
END_TABLE(FEOSSTDIO)

MAKE_FAKEMODULE(FEOSSTDIO)
