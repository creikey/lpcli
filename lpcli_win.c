#include <windows.h>
#include <conio.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#include "lpcli.h"

int lpcli_clipboardcopy(const char *text)
{
	const size_t len = strlen(text) + 1;
	HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), text, len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	if (!SetClipboardData(CF_TEXT, hMem))
	{
		return LPCLI_FAIL;
	}
	CloseClipboard();
	return LPCLI_OK;
}

// Todo: calculate the encoded length as characters entered
int lpcli_readpassword(const char *prompt, char *out, size_t outl)
{
	printf("%s", prompt);
	wchar_t input[MAX_INPUTWCS];
	wint_t c;
	int i;
	for (i = 0; i < MAX_INPUTWCS - 1; i++)
	{
		c = _getwch();
		if (c == L'\r')
		{
			//input[i++] = 0;
			break;
		}
		input[i] = c;
	}
	input[i] = 0;
	printf("\n");
	int err = 0;
	int len = WideCharToMultiByte(CP_UTF8, 0, input, i, out, outl - 1, NULL, NULL);
	SecureZeroMemory(input, sizeof input);
	
	if (len == 0 || len == 0xFFFD)
	{
		err = GetLastError();
	}
	out[len] = 0;
	if (err != 0)
	{
		fprintf(stderr, "WideCharToMultiByte got error code %i\n", err);
		return LPCLI_FAIL;
	}
	return LPCLI_OK;
}

static char **cmdline_to_argv_u8(int *argc)
{
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), argc);
	int i;
	int tlen = 0;
	int utf8len[*argc];
	int wlen[*argc];
	for (i = 0; i < *argc; i++)
	{
		wlen[i] = wcslen(wargv[i]) + 1;
		utf8len[i] = WideCharToMultiByte(CP_UTF8, 0, wargv[i], wlen[i], NULL, 0, NULL, NULL);
		tlen += utf8len[i];
	}
	int argvsize = (*argc + 1) * sizeof(char *);
	char *argvp = malloc(argvsize + tlen);
	char **argv = (void *) argvp;
	argvp += argvsize;
	for (i = 0; i < *argc; i++)
	{
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], wlen[i], argvp, utf8len[i], NULL, NULL);
		SecureZeroMemory(wargv[i], wlen[i]);
		argv[i] = argvp;
		argvp += utf8len[i];
	}
	argv[*argc] = NULL;
	LocalFree(wargv);
	return argv;
}

/*
void *lpcli_zeromemory(void *dst, size_t dstlen)
{
	return SecureZeroMemory(dst, dstlen);
}
*/

int main()
{
	setlocale(LC_ALL, "");
	int argc;
	char **argv = cmdline_to_argv_u8(&argc);
	int ret = lpcli_main(argc, argv);
	free(argv);
	return ret;
}
