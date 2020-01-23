/*
 * $Id: main.c,v 1.15 2004/09/18 19:11:46 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifdef WIN32
#include <process.h> /* _beginthreadex */
#endif

#include "as_ares.h"
#include "main.h"

/*****************************************************************************/

#ifdef WIN32

/* separate thread for reading console input in win32 */
unsigned int __stdcall console_input_func (void *data)
{
	char buf[1024*16];
	DWORD read;
	int written;
	int event_fd = (int) data;
	HANDLE console_d = GetStdHandle (STD_INPUT_HANDLE);

	SetConsoleMode (console_d, ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT |
	                ENABLE_ECHO_INPUT);

	while (ReadFile (console_d, buf, sizeof (buf) - 1, &read, NULL))
	{
		/* send entire line to event loop for processing */
		if (read == 0)
			continue;
		
		written = send (event_fd, buf, read, 0);

		if (written < 0 || written != (int)read)
		{
			/* FIXME: handle error */
		}
	}

	/* FIXME: quit event loop? */

	return 0;
}

#endif

void stdin_cb (int fd, input_id id, void *udata)
{
	static char buf[1024*16];
	static int buf_pos = 0;
	int read_buf, len;
	int argc;
	char **argv;

#ifdef WIN32
	if ((read_buf = recv (fd, buf + buf_pos, sizeof (buf) - buf_pos, 0)) < 0)
#else
	if ((read_buf = read (fd, buf + buf_pos, sizeof (buf) - buf_pos)) < 0)
#endif
		return;

	buf_pos += read_buf;

	/* check for LF */
	for (len = 0; len < buf_pos; len++)
		if (buf[len] == '\n')
			break;

	if (len == buf_pos)
	{
		/* panic if buffer is too small */
		assert (buf_pos < sizeof (buf));
		return;
	}

	buf[len++] = 0;

	/* handle cmd line, argv[] points into buf */
	parse_argv (buf, &argc, &argv);

	/* dispatch command */
	if (argc >  0)
		dispatch_cmd (argc, argv);

	free (argv);

	/* remove used data */
	memmove (buf, buf + len, buf_pos - len);
	buf_pos -= len;

#if 0
	/* print prompt */
	printf ("> ");
#endif
}

/*****************************************************************************/

int main (int argc, char *argv[])
{
	ASLogger *logger;
	int stdin_handle;
	input_id stdinput;

#ifdef WIN32
	HANDLE hThread;
	int fds[2];
#endif

	/* winsock init */
	tcp_startup ();

	/* setup logging */
	logger = as_logger_create ();
	as_logger_add_output (logger, "stderr");
	as_logger_add_output (logger, "ares.log");

	AS_DBG ("Logging subsystem started");

	/* setup event system */
	as_event_init ();

	/* init lib */
	if (!as_init ())
	{
		printf ("FATA: as_init() failed\n");
		exit (1);
	}

#ifdef WIN32
	/* create console reading thread on windows */
	if (socketpair (0, 0, 0, fds) < 0)
	{
		printf ("FATAL: socketpair() failed\n");
		exit (1);
	}

	stdin_handle = fds[1];
	hThread = (HANDLE) _beginthreadex (NULL, 0, console_input_func,
	                                   (void *)fds[0], 0, NULL);

	if (hThread == (HANDLE) -1 || hThread == (HANDLE) 0)
	{
		printf ("FATAL: couldn't start input thread\n");
		exit (1);
	}
#else
	stdin_handle = 0;
#endif

	/* add callback for command handling */
	stdinput = input_add (stdin_handle, NULL, INPUT_READ, stdin_cb, 0);

#if 0
	/* print prompt */
	printf ("> ");
#endif

	/* run event loop */
	AS_DBG ("Entering event loop");
	as_event_loop ();
	AS_DBG ("Left event loop");

	input_remove (stdinput);

#if WIN32
	/* terminate thread if it is still running and close thread handle */
	TerminateThread (hThread, 0);
	CloseHandle (hThread);
#endif

	/* cleanup  lib */
	as_cleanup ();

	/* shutdown */
	as_event_shutdown ();
	as_logger_free (logger);

	/* winsock shutdown */
	tcp_cleanup ();

	return 0;
}

/*****************************************************************************/

/* modifies cmdline */
int parse_argv(char *cmdline, int *argc, char ***argv)
{
	char *p, *token;
	int in_quotes, in_token;
	
	/* should always be enough */
	*argv = malloc (sizeof (char*) * strlen (cmdline) + 1);
	*argc = 0;

	in_token = in_quotes = 0;
	p = token = cmdline;

	for (;;)
	{
		switch (*p)
		{
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			if (in_token && !in_quotes)
			{
				*p = 0;
				(*argv)[*argc] = token;
				(*argc)++;
				in_token = 0;
				token = NULL;
			}
			break;

		case '\0':
			if (in_token)
			{
				(*argv)[*argc] = token;
				(*argc)++;
				in_token = 0;
				token = NULL;
			}

			return 0;

		case '"':
			if (in_token)
			{
				*p = 0;
				(*argv)[*argc] = token;
				(*argc)++;
				in_token = 0;			
				token = NULL;
			}

			in_quotes = !in_quotes;

			break;
		default:
			if (!in_token || (in_quotes && !in_token))
			{
				in_token = 1;
				token = p;
			}

			break;
		}

		p++;
	}	

	return 0;
}

/*****************************************************************************/
