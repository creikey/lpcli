#ifndef _WIN32
	#ifndef _XOPEN_SOURCE
	#define _XOPEN_SOURCE
	#endif
	#include <termios.h>
#else
	#include <windows.h>
	#include <conio.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
//#include <locale.h>

#include "lp.h"

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#define LP_FAIL EXIT_FAILURE

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#define LP_OK EXIT_SUCCESS


#define ERRORS \
	X(none, "Not an error") \
	X(unrecognized_options, "Unrecognized or incorrect options used") \
	X(cannot_set_to, "Cannot set %s value to %i") \
	X(clipboard, "Cannot copy to clipboard") \
	X(read_password, "Failed to read the password") \
	X(inc_exc ,"Character set inclusion and exclusion options cannot be used together") \


#define X(A, B) ERR_##A,
enum ErrorCodes { ERRORS };
#undef X

#define X(A, B) B "\n",
static const char *errstr[] = { ERRORS };
#undef X


void print_usage(void)
{
	fprintf(stderr,
	"Usage: lesspass <site> <login> [options] \n"
	"Options: \n"
	"  -l                  add lowercase in password \n"
	"  -u                  add uppercase in password \n"
	"  -d                  add digits in password \n"
	"  -s                  add symbols in password \n"
	"\n"
	"  --no-lowercase      remove lowercase from password \n"
	"  --no-uppercase      remove uppercase from password \n"
	"  --no-digits         remove digits from password \n"
	"  --no-symbols        remove symbols from password \n"
	"\n"
	"  --length, -L        int (default 16) \n"
	"  --counter, -c       int (default 1) \n"
	"\n"
	"  --clipboard, -C     copy to clipboard instead of displaying it. \n"
	"                      xclip (Linux) or clip (Windows) is required.\n"
	);
	fflush(stderr);
}

int print_error(const char * format, ...)
{
	fputs("Error: ", stderr);
	va_list args;
	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fflush(stderr);
	
	return LP_FAIL;
}

typedef struct parsed_args
{
	const char *site;
	const char *login;
	const char *password;
	unsigned charset_in;
	unsigned charset_ex;
	unsigned charset;
	int length;
	int counter;
	unsigned changed;
} OPTIONS;


static const OPTIONS default_opts =
{
	.site = NULL, .login = NULL, .password = NULL,
	.charset_in = 0, .charset_ex=LP_CSF_DEF, .charset = 0,
	.length = 0, .counter = 0, .changed = 0,
};

enum
{
	CMDLINE_CSETINC   = 0x01, // inclusive
	CMDLINE_CSETEXC   = 0x02, // exclusive
	CMDLINE_LENGTH    = 0x04,
	CMDLINE_COUNTER   = 0x08,
	CMDLINE_PASSWORD  = 0x10,
	CMDLINE_CLIPBOARD = 0x20
}; //changed flags

#define TOUCH(X, Y) (X->changed |= CMDLINE_##Y)


int read_args(int argc, const char **argv, OPTIONS *opts)
{
	if(argc < 3)
		return LP_FAIL;
	int i = 0;
	const char *ptr;
	char *ptrEnd;
	ptr = argv[++i];
	opts->site = ptr;
	ptr = argv[++i];
	opts->login = ptr;
	
	ptr = argv[++i];
	
	if(ptr && *ptr != '-')
	{
		opts->password = ptr;
		TOUCH(opts, PASSWORD);
		ptr = argv[++i];
	}
	
	int opt_open = 0;

	while(argv[i] != NULL)
	{
		if(!opt_open)
		{
			if(*ptr != '-')
				return LP_FAIL;
			ptr++;
		}
		
		switch(*ptr)
		{
			case '\0': // -
				if(!opt_open)
				{
					return LP_FAIL;
				}
				opt_open = 0;
				ptr = argv[++i];
				break;
			case '-': // --
				if(opt_open)
					return LP_FAIL;
				ptr++;
				if(strcmp(ptr, "no-lowercase") == 0)
				{
					opts->charset_ex &= ~ LP_CSF_L;
					TOUCH(opts, CSETEXC);
				}
				else if(strcmp(ptr, "no-uppercase") == 0)
				{
					opts->charset_ex &= ~ LP_CSF_U;
					TOUCH(opts, CSETEXC);
				}
				else if(strcmp(ptr, "no-digits") == 0)
				{
					opts->charset_ex &= ~ LP_CSF_D;
					TOUCH(opts, CSETEXC);
				}
				else if(strcmp(ptr, "no-symbols") == 0)
				{
					opts->charset_ex &= ~ LP_CSF_S;
					TOUCH(opts, CSETEXC);
				}
				else if(strcmp(ptr, "clipboard") == 0)
				{
					TOUCH(opts, CLIPBOARD);
				}
				else if(strcmp(ptr, "length") == 0)
				{
					ptr = argv[++i];
					if(ptr == NULL)
						return LP_FAIL;
					opts->length = strtol(ptr, &ptrEnd, 10);
					if(*ptrEnd != '\0')
						return LP_FAIL;
					TOUCH(opts, LENGTH);
				}
				else if(strcmp(ptr, "counter") == 0)
				{
					ptr = argv[++i];
					if(ptr == NULL)
						return LP_FAIL;
					opts->counter = strtol(ptr, &ptrEnd, 10);
					if(*ptrEnd != '\0')
						return LP_FAIL;
					TOUCH(opts, COUNTER);
				}
				else
				{
					return LP_FAIL;
				}
				ptr = argv[++i];
				break;
			case 'L':
				ptr++;
				if(*ptr == '\0')
				{
					ptr = argv[++i];
					if(ptr == NULL)
						return LP_FAIL;
				}
				opts->length = strtol(ptr, &ptrEnd, 10);
				//if(*ptrEnd != '\0')
				//	return 1;
				TOUCH(opts, LENGTH);
				ptr = (const char*) ptrEnd;  opt_open = 1;
				//ptr = argv[++i];
				//opt_open = 0;
				break;
			case 'c':
				ptr++;
				if(*ptr == '\0')
				{
					ptr = argv[++i];
					if(ptr == NULL)
						return LP_FAIL;
				}
				opts->counter = strtol(ptr, &ptrEnd, 10);
				//if(*ptrEnd != '\0')
				//	return 1;
				TOUCH(opts, COUNTER);
				ptr = (const char*) ptrEnd; opt_open = 1;
				//ptr = argv[++i];
				//opt_open = 0;
				break;
			case 'l':
				opts->charset_in |= LP_CSF_L;
				TOUCH(opts, CSETINC);
				ptr++;
				opt_open = 1;
				break;
			case 'u':
				opts->charset_in |= LP_CSF_U;
				TOUCH(opts, CSETINC);
				ptr++;
				opt_open = 1;
				break;
			case 'd':
				opts->charset_in |= LP_CSF_D;
				TOUCH(opts, CSETINC);
				ptr++;
				opt_open = 1;
				break;
			case 's':
				opts->charset_in |= LP_CSF_S;
				TOUCH(opts, CSETINC);
				ptr++;
				opt_open = 1;
				break;
			case 'C':
				TOUCH(opts, CLIPBOARD);
				ptr++;
				opt_open = 1;
				break;
			default:
				return LP_FAIL;
		}
	}
	return LP_OK;
}
#undef TOUCH


//FILE *popen(const char *command, const char *type);
//int pclose(FILE *stream);
int copy_to_clipboard(const char *text)
{
#ifndef _WIN32
	FILE *pout = popen("xclip -selection clipboard -quiet -loop 1", "w");
	if(!pout)
	{
		return LP_FAIL;
	}
	fprintf(pout, text);
	fflush(pout);
	pclose(pout);
#else
	const size_t len = strlen(text) + 1;
	HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(hMem), text, len);
	GlobalUnlock(hMem);
	OpenClipboard(0);
	EmptyClipboard();
	if(!SetClipboardData(CF_TEXT, hMem))
	{
		return LP_FAIL;
	}
	CloseClipboard();
#endif
	return LP_OK;
}

int read_password(const char *prompt, char *out, size_t outl)
{
	printf(prompt);
#ifndef _WIN32
	static struct termios told, tnew;
	tcgetattr(0, &told);
	tnew = told;
	tnew.c_lflag &= ~ICANON;
	tnew.c_lflag &= ~ECHO;
	tcsetattr(0, TCSANOW, &tnew);
	
	out = fgets(out, outl, stdin);
	tcsetattr(0, TCSANOW, &told);
	
	if(!out)
		return LP_FAIL;

	out[strcspn(out, "\r\n")] = 0;
#else
	char c;
	size_t i;
	for(i=0; i < outl - 1; i++)
	{
		c = getch();
		if(c == '\r')
		{
			printf("\n");
			break;
		}
		out[i] = c;
	}
	out[i] = '\0';
#endif
	return LP_OK;
}

#define ISOPTSET(X) (options.changed & CMDLINE_##X)

void print_options(OPTIONS *t)
{
	printf("Options: -");
	if(t->charset & LP_CSF_L)
		printf("l");
	if(t->charset & LP_CSF_U)
		printf("u");
	if(t->charset & LP_CSF_D)
		printf("d");
	if(t->charset & LP_CSF_S)
		printf("s");
	printf("c%u", t->counter);
	printf("L%u", t->length);
	printf("\n");
}

int main(int argc, const char **argv)
{
	//setlocale(LC_CTYPE, "en.UTF-8");
	OPTIONS options = default_opts;
	if(read_args(argc, argv, &options) != LP_OK)
	{
		return print_error(errstr[ERR_unrecognized_options]);
	}
	
	unsigned temp;
	if(ISOPTSET(CSETINC) && ISOPTSET(CSETEXC))
	{
		return print_error(errstr[ERR_inc_exc]);
	}

	LP_CTX *ctx = LP_CTX_new();
	
	if(ISOPTSET(CSETINC)) // this should never happen
	{
		temp = LP_set_charsets(ctx, options.charset_in);
		if(temp != options.charset_in)
		{
			LP_CTX_free(ctx);
			return print_error(errstr[ERR_cannot_set_to], "inclusive charset flags", options.charset_in);
		}
		options.charset = options.charset_in;
	}
	else if(ISOPTSET(CSETEXC))
	{
		temp = LP_set_charsets(ctx, options.charset_ex);
		if(temp != options.charset_ex)
		{
			LP_CTX_free(ctx);
			return print_error(errstr[ERR_cannot_set_to], "exclusive charset flags", options.charset_ex);
		}
		options.charset = options.charset_ex;
	}
	else
	{
		options.charset = LP_set_charsets(ctx, 0);
	}
	
	if(ISOPTSET(LENGTH))
	{
		temp = LP_set_length(ctx, options.length);
		if(temp != (unsigned) options.length)
		{
			LP_CTX_free(ctx);
			return print_error(errstr[ERR_cannot_set_to], "length", options.length);
		}
	}
	else
	{
		options.length = LP_set_length(ctx, 0);
	}
	
	if(ISOPTSET(COUNTER))
	{
		temp = LP_set_counter(ctx, options.counter);
		if(temp != (unsigned) options.counter)
		{
			LP_CTX_free(ctx);
			return print_error(errstr[ERR_cannot_set_to], "counter", options.counter);
		}
	}
	else
	{
		options.counter = LP_set_counter(ctx, 0);
	}
	
	char genpass[options.length + 1];
	genpass[options.length] = 0;
	
	//print_options(&options);
	
	if(!ISOPTSET(PASSWORD))
	{

		char passwd_in[1024];
		if(read_password("Password: ", passwd_in, sizeof passwd_in) != LP_OK)
		{
			LP_CTX_free(ctx);
			return print_error(errstr[ERR_read_password]);
		}
		printf("\n");
		LP_get_pass(ctx, options.site, options.login, (const char *) passwd_in, genpass, sizeof genpass);
		//mymemset(passwd_in, 0, sizeof passwd_in);
	}
	else
	{
		LP_get_pass(ctx, options.site, options.login, options.password, genpass, sizeof genpass);
	}
	
	int do_copy = ISOPTSET(CLIPBOARD);
	//mymemset(&options, 0, sizeof options);
	
	LP_CTX_free(ctx);
	
	if(do_copy)
	{
		if(copy_to_clipboard(genpass) != LP_OK)
			return print_error(errstr[ERR_clipboard]);
	}
	else
	{
		printf("%s\n", genpass);
	}
	
	//mymemset(&genpass, 0, sizeof genpass);
	return EXIT_SUCCESS;
}
