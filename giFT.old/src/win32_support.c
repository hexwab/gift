/*
 * win32_support.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifdef WIN32

#include <windows.h>
#include <winsock2.h>

#include "gift.h"

#include "win32_support.h"

/*****************************************************************************/

/* WinSock version */
/* I am not sure if 1.1 is the correct version to use */
#define WS_MAJOR 2
#define WS_MINOR 0

#define LOG_WINDOW_CLASS "giFT_log"
#define LOG_WINDOW_TITLE "giFT"
#define ERROR_DIALOG_TITLE "giFT error"

/*****************************************************************************/

/* the directory in which the giFT executable lies */
static char *gift_dir = NULL;

/* the giFT data directory */
static char *data_dir = NULL;

static HWND log_window;
static HWND log_control;
static HWND input_window;

/*****************************************************************************/

static void back_to_forward_slashes (char *str)
{
	for (; *str; str++) if (*str == '\\') *str = '/';
}

char *win32_gift_dir ()
{
	return gift_dir;
}

char *win32_data_dir ()
{
	return data_dir;
}

/*
 * initialize win32 stuff
 */
static void win32_init (char *progname)
{
	WSADATA wsa_data;
	char *slash;

	/* initialize gift_dir */
	if (!(slash = strrchr (progname, '\\')))
		slash = strrchr (progname, '/');

	assert (slash);

	gift_dir = malloc (slash - progname + 1);
	strncpy (gift_dir, progname, slash - progname);
	gift_dir[slash - progname] = '\0';
	back_to_forward_slashes (gift_dir);

	/* initialize data_dir */
	data_dir = malloc (strlen (gift_dir) + 6); /* data/\0 */
	sprintf (data_dir, "%s/data", gift_dir);

	/* initialize WinSock */
	if (WSAStartup (MAKEWORD (WS_MAJOR, WS_MINOR), &wsa_data) != 0)
		GIFT_FATAL (("Failed to initialize WinSock"));
}

void win32_cleanup ()
{
}

static void log_window_resize_controls ()
{
	RECT rt;
	
	GetClientRect (log_window, &rt);
	SetWindowPos (log_control, HWND_TOP, 0, 0, rt.right, rt.bottom, 0);
	UpdateWindow (log_window);
}

static LRESULT CALLBACK log_window_proc (HWND window, UINT message,
					 WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_SIZE:
	case WM_SIZING:
		log_window_resize_controls ();
		break;
	case WM_DESTROY:
		PostQuitMessage (0);
		break;
	default:
		return DefWindowProc (window, message, wparam, lparam);
	}
	return 0;
}

static void log_window_create (HINSTANCE instance)
{
	WNDCLASSEX wc;

	wc.cbSize = sizeof (wc);
	
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC) log_window_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	wc.hIcon = NULL; /* LoadIcon (instance, (LPCTSTR) IDI_MINIRC); */
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = LOG_WINDOW_CLASS;
	wc.hIconSm = NULL; /* LoadIcon (instance, (LPCTSTR) IDI_SMALL); */
	
	RegisterClassEx (&wc);
	
	log_window = CreateWindowEx (0, LOG_WINDOW_CLASS, LOG_WINDOW_TITLE,
				     WS_EX_OVERLAPPEDWINDOW | WS_SIZEBOX
				     | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
				     | WS_SYSMENU, CW_USEDEFAULT, 0,
				     500, 500, NULL, NULL, instance, NULL);
	
	log_control = CreateWindowEx (0, "EDIT", NULL,
				      WS_CHILD
				      | WS_VISIBLE | WS_VSCROLL
				      | ES_READONLY | ES_MULTILINE,
				      0, 0, 0, 0,
				      log_window, NULL,
				      instance, NULL);
	
	log_window_resize_controls ();
}

static void log_window_show ()
{
	ShowWindow (log_window, SW_SHOW);
	ShowWindow (log_control, SW_SHOW);
	UpdateWindow (log_window);
}

void win32_printf (const char *format, ...)
{
	DWORD sel_start, sel_end;
	int len;
	static char buf[4096];
	va_list args;
	
	va_start (args, format);
	vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	/* Thanks to Miranda ICQ for this */

	len = GetWindowTextLength (log_control);	
	SendMessage (log_control, EM_GETSEL, (WPARAM) &sel_start,
		     (LPARAM) &sel_end);
        SendMessage (log_control, EM_SETSEL, len, len);
	SendMessage (log_control, EM_REPLACESEL, FALSE, (LPARAM) buf);
        SendMessage (log_control, EM_SETSEL, (WPARAM) sel_start,
		     (LPARAM) sel_end);
        SendMessage (log_control, EM_LINESCROLL, 0,
		     SendMessage (log_control, EM_GETLINECOUNT, 0, 0));
        
	InvalidateRect (log_control, NULL, FALSE);
}

void win32_fatal (const char *format, ...)
{
	char buf[4096];
	va_list args;
	
	va_start (args, format);
	vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	MessageBox (log_window ? log_window : NULL, buf, "giFT error",
		    MB_OK | MB_ICONERROR);

	ExitProcess (1);
}

static int message_loop ()
{
	MSG msg;

	while (GetMessage (&msg, NULL, 0, 0)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	return msg.wParam;
}

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
		    LPSTR cmd_line, int cmd_show)
{
	int result;
	const char *progname;

	log_window_create (instance);

	log_window_show ();

	GIFT_DEBUG (("command line: %s", GetCommandLine ()));

	progname = GetCommandLine ();
	if (progname[0] == '"') progname++;
	win32_init (progname);

	real_main (0, NULL);

	result = message_loop ();

	return result;
}

void win32_gtod (long *sec, long *usec)
{
	SYSTEMTIME st;
	GetLocalTime (&st);
	*sec = st.wSecond;
	*usec = st.wMilliseconds;
}

#endif /* WIN32 */
