/*
 * $Id: ar_callback.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "aresdll.h"
#include "as_ares.h"

/*****************************************************************************/

#define AR_WND_CLASS_NAME "AresDLL wndclass"
#define AR_WND_TITLE "AresDLL cb window"
#define AR_WND_CB_MSG (WM_USER + 1)

typedef struct
{
	ARCallbackCode code;
	void *param1;
	void *param2;
} ASCallbackData;

static HWND callback_window = NULL;
static ARCallback callback_func = NULL;
static int callback_active = 0; /* if > 0 a callback is active */

/*****************************************************************************/

static long FAR PASCAL callback_wnd_proc (HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam)
{
	if (message == AR_WND_CB_MSG)
	{
		ASCallbackData *data = (ASCallbackData*)wParam;
		
		callback_func (data->code, data->param1, data->param2);
		return 0;
	}

    /* Let windows handle the message */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*****************************************************************************/

as_bool ar_create_callback_system (ARCallback callback)
{
    WNDCLASSEX wc;

	if (callback_window != NULL)
		return FALSE;

	assert (callback_func == NULL);
	callback_func = callback;

    /* Fill in the window class structure */
    wc.cbSize        = sizeof (WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)callback_wnd_proc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle (NULL);
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = AR_WND_CLASS_NAME;
    wc.hIconSm       = NULL;

    /* Register the window class */
    if (!RegisterClassEx (&wc))
    {
        WNDCLASS wndclass;

		/* If there already is a class instance that's fine */
        if (!GetClassInfo (GetModuleHandle (NULL), AR_WND_CLASS_NAME, &wndclass))
        {
			AS_ERR ("Coulnd't register window class for callback window");
			return FALSE;
        }
    }

    /* Create the window */
    callback_window = CreateWindowEx (0, 
	                                  AR_WND_CLASS_NAME,
	                                  AR_WND_TITLE,
	                                  CS_NOCLOSE, /* style */
	                                  0,0,0,0, /* position */
	                                  NULL, /* parent window */
	                                  NULL, /* menu */
	                                  GetModuleHandle (NULL),
	                                  NULL);

    if (!callback_window)
    {
		AS_ERR ("Coulnd't create callback window");
		return FALSE;
    }

#if 0
    /* Hide window */
    ShowWindow (callback_window, SW_HIDE);
#endif

	return TRUE;
}

as_bool ar_destroy_callback_system ()
{
	if (callback_window == NULL)
		return FALSE;

	/* Destroy window */
    DestroyWindow (callback_window);
	callback_window = NULL;
	callback_func = NULL;

	/* Remove window class */
	UnregisterClass (AR_WND_CLASS_NAME, GetModuleHandle (NULL));

	return TRUE;
}

/*****************************************************************************/

/* Call user in his own thread. */
as_bool ar_raise_callback (ARCallbackCode code, void *param1, void *param2)
{
	ASCallbackData data;

	assert (callback_window != NULL);
	assert (callback_func != NULL);

	callback_active++;

	data.code = code;
	data.param1 = param1;
	data.param2 = param2;

	/* Send message to window proc in other thread. This blocks until the
	 * callback has finished.
	 */
	SendMessage (callback_window, AR_WND_CB_MSG, (WPARAM)&data, 0);

	callback_active--;

	return TRUE;
}

as_bool ar_callback_active ()
{
	return (callback_active > 0);
}

/*****************************************************************************/
