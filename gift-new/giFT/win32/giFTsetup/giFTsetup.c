/*
 * giFTsetup.c
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

/*
	TODO Center the dialog box on startup
	TODO Fix the "Back" button being grayed out if the user causes the error ()
	     dialog box to appear (i.e., we called PropSheet_SetCurSel)
	TODO Change the focus to the radio button that's been selected on startup
	TODO The port fields should be enabled if the user checks the
	     IDC_SPECIFY_PORTS radio button, and disabled otherwise

*/

#include <gift.h>
#include <file.h>
#include <config.h>

#include <prsht.h>
#include <shlobj.h>

#include "resource.h"

/*****************************************************************************/

#define	APP_NAME "giFTsetup"

/* choose a random port between 1215 and 65535 */
static unsigned short LOW_PORT  = GIFT_INTERFACE_PORT + 2;
static unsigned short HIGH_PORT = 65535U;

enum
{
	PAGE_INTRO,
	PAGE_FIREWALL,
	PAGE_NODE_CLASS,
	PAGE_DOWNLOAD_DIR,
	PAGE_SHARED_DIRS,
	PAGE_FINISH
};

#define PAGE_COUNT (PAGE_FINISH + 1)

/*****************************************************************************/

static HINSTANCE  app_instance;
static HFONT      title_font;
static Config    *gift_conf       = NULL;
static Config    *openft_conf	  = NULL;
static int        finished        = FALSE;
static int        behind_firewall = FALSE;
static int        main_setup      = 0;

/*****************************************************************************/

#define MAIN_SETUP         "main/setup"
#define DOWNLOAD_COMPLETED "download/completed"
#define SHARING_ROOT       "sharing/root"

#define MAIN_CLASS		   "main/class"
#define MAIN_PORT          "main/port"
#define MAIN_HTTP_PORT     "main/http_port"

/* 1 if user has specified specific ports to use, otherwise 0 */
#define MAIN_FIREWALLED    "main/firewalled"

/*****************************************************************************/

#ifdef _DEBUG
#define DBG(x) dbg (x)

static void dbg (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	OutputDebugString (buf);
	OutputDebugString ("\n");
}
#else
#define DBG(x)
#endif

/*****************************************************************************/

static void error (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	MessageBox (NULL, buf, APP_NAME, MB_OK | MB_ICONERROR);
}

static void fatal_error (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	MessageBox (NULL, buf, APP_NAME, MB_OK | MB_ICONERROR);

	ExitProcess (2);	/* 2 = error, 'cause 1 = finished */
}

static void wdbg (const char *format, ...)
{
	char buf[4096];
	va_list args;

	va_start (args, format);
	_vsnprintf (buf, sizeof (buf) - 1, format, args);
	va_end (args);

	OutputDebugString (buf);
	OutputDebugString ("\n");
}

/*****************************************************************************/

static HFONT create_font (const char *name, int size, int bold,
						  int italic)
{
    HDC dc;
	int screen_log_pixels;

    dc = GetDC (NULL);
    screen_log_pixels = GetDeviceCaps (dc, LOGPIXELSY);
    ReleaseDC (NULL, dc);

	return CreateFont (-MulDiv (size, screen_log_pixels, 72),
	                   0, 0, 0, bold ? FW_BOLD : FW_REGULAR,
	                   italic, FALSE, FALSE, DEFAULT_CHARSET,
	                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
	                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
	                   name);
}

static void destroy_font (HFONT font)
{
	DeleteObject (font);
}

/*****************************************************************************/

static const char *browse_for_directory (HWND owner)
{
	static char buffer[MAX_PATH];
	BROWSEINFO bi;
	LPITEMIDLIST id_list;
	LPMALLOC shmalloc;

	ZeroMemory (&bi, sizeof (bi));
	bi.hwndOwner      = owner;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = buffer;
	bi.lpszTitle      = "Browse...";
	bi.ulFlags        = BIF_RETURNONLYFSDIRS;
	bi.lpfn           = NULL;
	bi.lParam         = 0;

	id_list = SHBrowseForFolder (&bi);

	if (id_list)
	{
		SHGetPathFromIDList (id_list, buffer);

		SHGetMalloc (&shmalloc);
		shmalloc->lpVtbl->Free (shmalloc, id_list);
		shmalloc->lpVtbl->Release (shmalloc);

		return buffer;
	}
	else
	{
		return NULL;
	}
}

/*****************************************************************************/

static BOOL CALLBACK dp_intro (HWND window, UINT message,
							   WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	 case WM_NOTIFY:
	 {
		LPNMHDR nh = (LPNMHDR) lparam;

		switch (nh->code)
		{
		 case PSN_SETACTIVE:
			PostMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
			             PSWIZB_NEXT);
			break;
		 case PSN_WIZNEXT:
			SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
			             PSWIZB_BACK | PSWIZB_NEXT);
			break;
		 default:
			return FALSE;
		}

		break;
	 }
	 case WM_INITDIALOG:
		SendMessage (GetDlgItem (window, IDC_TITLE_TEXT),
		             WM_SETFONT, (WPARAM) title_font,
		             MAKELPARAM (FALSE, 0));
		break;
	 default:
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static unsigned short get_random_port ()
{
	int port;

	/* 0 - 65535 */
	srand ((unsigned int) GetTickCount ());

	do
		port = rand () * 2 + rand () % 2;
	while (port < LOW_PORT || port > HIGH_PORT);

	return port;
}

static BOOL CALLBACK dp_firewall (HWND window, UINT message,
								  WPARAM wparam, LPARAM lparam)
{
	UINT openft_port = 0;
	UINT http_port   = 0;
	UINT firewalled  = 0;

	switch (message)
	{
	 case WM_NOTIFY:
	 {
		LPNMHDR nh = (LPNMHDR) lparam;

		switch (nh->code)
		{
		 case PSN_SETACTIVE:
		 case PSN_WIZBACK:
			SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
			             PSWIZB_BACK | PSWIZB_NEXT);
			break;
		 case PSN_WIZNEXT:
			if (IsDlgButtonChecked(window, IDC_BEHIND_FIREWALL)
			    == BST_CHECKED)
			{
				SetWindowLong (window, DWL_MSGRESULT, -1);
				PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_NODE_CLASS);

				/* 1 = user class */
				config_set_int (openft_conf, MAIN_CLASS, 1);
				config_set_int (openft_conf, MAIN_PORT, 0);
				config_set_int (openft_conf, MAIN_HTTP_PORT, 0);
				config_set_int (openft_conf, MAIN_FIREWALLED, 1);

				behind_firewall = TRUE;
			}
			else
			{
				BOOL rv;

				behind_firewall = FALSE;

				openft_port = GetDlgItemInt (window, IDC_EDIT_OPENFT_PORT,
											 &rv, FALSE);
				if (!rv || openft_port < LOW_PORT || openft_port > HIGH_PORT)
				{
					error ("Please enter an OpenFT port between %hu and %hu",
					       LOW_PORT, HIGH_PORT);
					SetWindowLong (window, DWL_MSGRESULT, -1);
					PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_FIREWALL - 1);
					SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
					             PSWIZB_BACK | PSWIZB_NEXT);
					return FALSE;
				}

				if (openft_port == http_port)
				{
					error ("Please enter different port numbers");
					SetWindowLong (window, DWL_MSGRESULT, -1);
					PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_FIREWALL - 1);
					return FALSE;
				}

				http_port = GetDlgItemInt (window, IDC_EDIT_HTTP_PORT,
				                           &rv, FALSE);
				if (!rv || http_port < LOW_PORT || http_port > HIGH_PORT)
				{
					error ("Please enter an HTTP port between %hu and %hu",
					       LOW_PORT, HIGH_PORT);
					SetWindowLong (window, DWL_MSGRESULT, -1);
					PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_FIREWALL - 1);
					return FALSE;
				}

				config_set_int (openft_conf, MAIN_PORT, openft_port);
				config_set_int (openft_conf, MAIN_HTTP_PORT, http_port);

				if (IsDlgButtonChecked(window, IDC_NOT_BEHIND_FIREWALL)
				    == BST_CHECKED)
					config_set_int (openft_conf, MAIN_FIREWALLED, 0);
				else
					config_set_int (openft_conf, MAIN_FIREWALLED, 1);
			}

			break;

		 default:
			return FALSE;
		}

		break;
	 }

	 case WM_INITDIALOG:
	 {
		int class;

		openft_port = config_get_int (openft_conf, MAIN_PORT);
		http_port   = config_get_int (openft_conf, MAIN_HTTP_PORT);
		firewalled  = config_get_int (openft_conf, MAIN_FIREWALLED);

		while (!openft_port || openft_port == http_port)
			openft_port = get_random_port ();

		while (!http_port || http_port == openft_port)
			http_port = get_random_port ();

		SetDlgItemInt (window, IDC_EDIT_OPENFT_PORT, openft_port, FALSE);
		SetDlgItemInt (window, IDC_EDIT_HTTP_PORT, http_port, FALSE);

		SendDlgItemMessage (window, IDC_EDIT_OPENFT_PORT, EM_LIMITTEXT, 5, 0);
		SendDlgItemMessage (window, IDC_EDIT_HTTP_PORT, EM_LIMITTEXT, 5, 0);

		class = config_get_int (openft_conf, MAIN_CLASS);

		if (openft_port == 0 || http_port == 0)
		{
			if (!firewalled)
				CheckDlgButton (window, IDC_NOT_BEHIND_FIREWALL, BST_CHECKED);
			else
				CheckDlgButton (window, IDC_BEHIND_FIREWALL, BST_CHECKED);
			/* TODO -- disable port fields */
		}
		else if (!firewalled)
		{
			CheckDlgButton (window, IDC_NOT_BEHIND_FIREWALL, BST_CHECKED);
			/* TODO -- disable port fields */
		}
		else
		{
			CheckDlgButton (window, IDC_SPECIFY_PORTS, BST_CHECKED);
		}

		break;
	 }

	 default:
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static BOOL CALLBACK dp_node_class (HWND window, UINT message,
									WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	 case WM_NOTIFY:
	 {
		LPNMHDR nh = (LPNMHDR) lparam;

		switch (nh->code)
		{
		 case PSN_SETACTIVE:
		 case PSN_WIZBACK:
			SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
						 PSWIZB_BACK | PSWIZB_NEXT);
			break;
		 case PSN_WIZNEXT:
			if (IsDlgButtonChecked (window, IDC_RADIO_USER))
				config_set_int (openft_conf, "main/class", 1);
			else
			if (IsDlgButtonChecked (window, IDC_RADIO_SEARCH))
				config_set_int (openft_conf, "main/class", 3);
			else
			if (IsDlgButtonChecked (window, IDC_RADIO_INDEX))
				config_set_int (openft_conf, "main/class", 5);
			else
			if (IsDlgButtonChecked (window, IDC_RADIO_SEARCH_INDEX))
				config_set_int (openft_conf, "main/class", 7);
			else
				assert (0);

			 break;
		 default:
			return FALSE;
		}

		break;
	}

	 case WM_INITDIALOG:
	 {
		int class = config_get_int (openft_conf, "main/class");

		switch (class)
		{
		 case 3:
			CheckDlgButton (window, IDC_RADIO_SEARCH, BST_CHECKED);
			break;
		 case 5:
			CheckDlgButton (window, IDC_RADIO_INDEX, BST_CHECKED);
			break;
		 case 7:
			CheckDlgButton (window, IDC_RADIO_SEARCH_INDEX, BST_CHECKED);
			break;
		 case 1:
		 default:
			CheckDlgButton (window, IDC_RADIO_USER, BST_CHECKED);
			break;
		}

		break;
	 }

	 default:
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static BOOL CALLBACK dp_download_dir (HWND window, UINT message,
									  WPARAM wparam, LPARAM lparam)
{
	char completed[_MAX_PATH] = {'\0'};

	switch (message)
	{
	 case WM_NOTIFY:
	 {
		LPNMHDR nh = (LPNMHDR) lparam;

		switch (nh->code)
		{
		 case PSN_SETACTIVE:
		 case PSN_WIZBACK:
			if (behind_firewall)
			{
				SetWindowLong (nh->hwndFrom, DWL_MSGRESULT, -1);
				PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_NODE_CLASS);
			}
			SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
			             PSWIZB_BACK | PSWIZB_NEXT);
			break;
		 case PSN_WIZNEXT:
		 {
			DWORD attr;

			GetDlgItemText (window, IDC_DOWNLOAD_DIR, completed,
							sizeof (completed));

			if (!completed || !*completed)
			{
				error ("Please select a download directory");
				SetWindowLong (window, DWL_MSGRESULT, -1);
				PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_DOWNLOAD_DIR - 1);
				return FALSE;
			}

			attr = GetFileAttributes (completed);
			if (attr == -1)
			{
				if (!CreateDirectory (completed, NULL))
				{
					error ("Unable to create directory '%s': %s", completed,
					       platform_error ());
					SetWindowLong (window, DWL_MSGRESULT, -1);
					PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_DOWNLOAD_DIR - 1);
					return FALSE;
				}
			}
			else
			{
				if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
				{
					error ("'%s' is not a directory.", completed);
					SetWindowLong (window, DWL_MSGRESULT, -1);
					PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_DOWNLOAD_DIR - 1);
					return FALSE;
				}

				if (attr & FILE_ATTRIBUTE_READONLY)
				{
					error ("The directory '%s' is read only.", completed);
					SetWindowLong (window, DWL_MSGRESULT, -1);
					PropSheet_SetCurSel (nh->hwndFrom, NULL, PAGE_DOWNLOAD_DIR - 1);
					return FALSE;
				}
			}

			config_set_str (gift_conf, DOWNLOAD_COMPLETED,
			                file_unix_path (completed));

			break;
		 }

		 default:
			return FALSE;
		}

		break;
	 }

	 case WM_COMMAND:
		switch (LOWORD (wparam))
		{
		 case IDC_BROWSE:
		 {
			const char *dir;

			dir = browse_for_directory (window);

			if (dir)
				SetDlgItemText (window, IDC_DOWNLOAD_DIR, dir);
			break;
		 }
		 default:
			return FALSE;
		}

		break;

	 case WM_INITDIALOG:
		{
			char *dir;
			int free_me = 0;

			dir = config_get_str (gift_conf, DOWNLOAD_COMPLETED);
			if (dir)
			{
				dir = file_host_path (dir);
				++free_me;
			}
			else
			{
				_snprintf (completed, sizeof (completed), "%s\\%s",
				           platform_home_dir (), "completed");
				dir = completed;
			}

			SendDlgItemMessage (window, IDC_DOWNLOAD_DIR, EM_LIMITTEXT,
			                    _MAX_PATH, 0);

			SetDlgItemText (window, IDC_DOWNLOAD_DIR, dir);

			if (free_me)
				free (dir);

			break;
		}
	 default:
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static BOOL CALLBACK dp_shared_dirs (HWND window, UINT message,
									 WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	 case WM_NOTIFY:
	 {
		LPNMHDR nh = (LPNMHDR) lparam;

		switch (nh->code)
		{
		 case PSN_SETACTIVE:
		 case PSN_WIZBACK:
			SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
						 PSWIZB_BACK | PSWIZB_NEXT);
			break;
		 case PSN_WIZNEXT:
			 {
				HWND lb_wnd = GetDlgItem (window, IDC_SHARED_DIRS_LIST);
				DWORD count = SendMessage(lb_wnd, LB_GETCOUNT, 0, 0);
				DWORD index;
				DWORD all_len = 0;
				char *shared_dirs;

				for (index = 0; index < count; index++)
					all_len += SendMessage (lb_wnd, LB_GETTEXTLEN, index, 0) + 1;

				shared_dirs = malloc (all_len + 1);
				*shared_dirs = '\0';

				for (index = 0; index < count; index++)
				{
					DWORD len = SendMessage (lb_wnd, LB_GETTEXTLEN, index, 0);
					if (len)
					{
						char *unix_path;
						char* host_path = malloc (len + 1);

						SendMessage (lb_wnd, LB_GETTEXT, index, (LPARAM) host_path);

						unix_path = file_unix_path (host_path);
						free (host_path);

						if (*shared_dirs)
							strncat (shared_dirs, ":", all_len);

						strncat (shared_dirs, unix_path, all_len);
						free (unix_path);
					}
				}

				config_set_str (gift_conf, SHARING_ROOT, shared_dirs);

				free (shared_dirs);
			 }

			break;
		 default:
			return FALSE;
		}

		break;
	}

	 case WM_COMMAND:
		switch (LOWORD (wparam))
		{
		 case IDC_ADD:
		 {
			const char *dir;
			HWND lb_wnd = GetDlgItem (window, IDC_SHARED_DIRS_LIST);

			dir = browse_for_directory (window);

			if (dir)
				SendMessage (lb_wnd, LB_ADDSTRING, 0, (LPARAM) dir);

			break;
		 }

		 case IDC_REMOVE:
		 {
			int index;
			HWND lb_wnd = GetDlgItem (window, IDC_SHARED_DIRS_LIST);

			index = SendMessage (lb_wnd, LB_GETCURSEL, 0, 0);

			if (index != LB_ERR)
				SendMessage (lb_wnd, LB_DELETESTRING, (WPARAM) index, 0);
		 }
		 default:
			return FALSE;
		}

		break;

	 case WM_INITDIALOG:
		{
			char *sharing_root, *sharing_root0;
			char *unix_path;
			HWND lb_wnd = GetDlgItem (window, IDC_SHARED_DIRS_LIST);

			sharing_root = config_get_str (gift_conf, SHARING_ROOT);
			if (!sharing_root)
				break;

			sharing_root0 = sharing_root = STRDUP (sharing_root);

			while ((unix_path = string_sep (&sharing_root, ":")))
			{
				char *host_path = file_host_path (unix_path);

				SendMessage (lb_wnd, LB_ADDSTRING, 0, (LPARAM) host_path);
				free (host_path);
			}

			free (sharing_root0);
#if 0
			/* sharing of the completed dir will be incorporated into the daemon */
			if (!SendMessage(lb_wnd, LB_GETCOUNT, 0, 0))
				SendMessage (lb_wnd, LB_ADDSTRING, 0, (LPARAM) completed);
#endif

			break;
		}
	 default:
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static BOOL CALLBACK dp_finish (HWND window, UINT message,
								WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	 case WM_NOTIFY:
	 {
		LPNMHDR nh = (LPNMHDR) lparam;

		switch (nh->code)
		{
		 case PSN_SETACTIVE:
			PostMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
			             PSWIZB_BACK | PSWIZB_FINISH);
			break;
		 case PSN_WIZBACK:
			SendMessage (nh->hwndFrom, PSM_SETWIZBUTTONS, 0,
			             PSWIZB_BACK | PSWIZB_NEXT);
			break;

		case PSN_WIZFINISH:
			finished = TRUE;
			break;

		default:
			return FALSE;
		}

		break;
	}
	 case WM_INITDIALOG:
		SendMessage (GetDlgItem (window, IDC_TITLE_TEXT),
		             WM_SETFONT, (WPARAM) title_font,
		             MAKELPARAM (FALSE, 0));
		break;
	 default:
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static HPROPSHEETPAGE create_page (int id,
                                   DLGPROC dialog_proc,
                                   int exterior,
                                   const char *header_title,
                                   const char *header_sub_title)
{
	PROPSHEETPAGE psp;

	ZeroMemory (&psp, sizeof (psp));
	psp.dwSize = sizeof (psp);

	if (exterior)
		psp.dwFlags = PSP_USETITLE | PSP_HIDEHEADER;
	else
		psp.dwFlags = PSP_USETITLE | PSP_USEHEADERTITLE |
		              PSP_USEHEADERSUBTITLE;

	psp.hInstance         = app_instance;
	psp.pszTemplate       = (LPCSTR) id;
	psp.pszTitle          = "giFT Configuration Wizard"
#ifdef DEBUG
	" (" __DATE__ " " __TIME__ ")"
#endif
	;
	psp.pfnDlgProc        = dialog_proc;
	psp.lParam            = 0;
	psp.pszHeaderTitle    = header_title;
	psp.pszHeaderSubTitle = header_sub_title;

	return CreatePropertySheetPage (&psp);
}

/*****************************************************************************/

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prev_instance,
					LPSTR cmd_line, int cmd_show)
{
	PROPSHEETHEADER psh;
	HPROPSHEETPAGE  pages[PAGE_COUNT];
	HANDLE          mutex;

	/* only allow one instance of giFTsetup */
	mutex = CreateMutex (NULL, TRUE, APP_NAME);
	if (GetLastError () == ERROR_ALREADY_EXISTS)
		return 2;
	else
		if (mutex == NULL)
			fatal_error ("CreateMutex failed: %s", platform_error ());

	platform_init ("");

	app_instance = instance;

	if (!(gift_conf = gift_config_new ("giFT")))
		fatal_error ("Couldn't open gift.conf");

	if (!(openft_conf = gift_config_new ("OpenFT")))
		fatal_error ("Couldn't open OpenFT.conf");

	main_setup = config_get_int (gift_conf, MAIN_SETUP);

	title_font = create_font ("Verdana", 12, TRUE, FALSE);

	pages[PAGE_INTRO] = create_page (IDD_INTRO, dp_intro, TRUE, NULL, NULL);
	pages[PAGE_FIREWALL] = create_page (
		IDD_FIREWALL, dp_firewall, FALSE,
		"Firewall Configuration",
		"Are you behind a firewall?");
	pages[PAGE_NODE_CLASS] = create_page (
		IDD_NODE_CLASS, dp_node_class, FALSE,
		"Node Class",
		"Select the kind of node you want to be");
	pages[PAGE_DOWNLOAD_DIR] = create_page (
		IDD_DOWNLOAD_DIR, dp_download_dir, FALSE,
		"Download Directory",
		"Select the place to store your downloaded files");
	pages[PAGE_SHARED_DIRS] = create_page (
		IDD_SHARED_DIRS, dp_shared_dirs, FALSE,
		"Shared Directories",
		"Select the directories you want to share");
	pages[PAGE_FINISH] = create_page (IDD_FINISH, dp_finish, TRUE, NULL, NULL);

	ZeroMemory (&psh, sizeof (psh));

	psh.dwSize         = sizeof (psh);
	psh.dwFlags        = PSH_WIZARD97 | PSH_HEADER | PSH_WATERMARK;
	psh.hwndParent     = NULL;
	psh.hInstance      = instance;
	psh.pszCaption     = "giFT Configuration Wizard";
	psh.nPages         = PAGE_COUNT;
	psh.phpage         = pages;
	psh.pszbmWatermark = (LPCSTR) IDB_WATERMARK;
	psh.pszbmHeader    = (LPCSTR) IDB_HEADER;

	PropertySheet (&psh);

	if (finished)
	{
		config_set_int (gift_conf, MAIN_SETUP, 1);
		config_write (gift_conf);
		config_write (openft_conf);
	}

	config_free (gift_conf);
	config_free (openft_conf);

	destroy_font (title_font);

	ReleaseMutex (mutex);

	/* return 1 if successful, otherwise 0 */
	ExitProcess (finished);

	return finished;
}
