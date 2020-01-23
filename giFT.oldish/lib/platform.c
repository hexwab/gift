/*
 * platform.c - portability routines
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

#include <errno.h>

#include "gift.h"

/* only strictly required for UNIX, but we'll make Windows do it too */
#include "file.h"

#include "event.h"
#include "network.h"

#ifndef WIN32
# include <signal.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/utsname.h>
# include <sys/wait.h>
#else /* WIN32 */
# include <process.h> /* _beginthreadex() */
# include <sys/timeb.h>
# define WS_MAJOR 2
# define WS_MINOR 0
# define AF_LOCAL 1
# define R_OK 04
# define GIFT_REG_KEY "Software\\giFT\\giFT"   /* NSIS defines this for us */
# define GIFT_REG_SUBKEY "instpath"            /* ... */
#endif /* !WIN32 */

/* TODO -- this will be removed when rossta moves back to using pipe for
 * UNIX hosts */
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

/*****************************************************************************/

/**
 * Contains installed data, such as mime.types.
 *
 * UNIX:    this is defined by configure as DATA_DIR
 * Windows: The plugin_dir + /data
 */
static char *data_dir = NULL;

/**
 * Contains all dynamically loaded plugins that this giFT installation has
 * available.
 *
 * UNIX:    this is defined by configure as PLUGIN_DIR
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_LOCAL_MACHINE\Software\giFT\giFT\instpath registry key
 *          2. Directory where giFT started from
 *          3. Current directory
 */
static char *plugin_dir = NULL;

/**
 * Contains the user's 'home' directory.
 *
 * UNIX:    assigned to $HOME or /home/$USER
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_CURRENT_USER\Software\giFT\giFT\instpath registry key
 *          2. %HOME% directory, if %HOME% is defined in the environment.
 *          3. Directory where giFT started from.
 *          4. Current directory.
 */
static char *home_dir = NULL;

/**
 * Contains all user preferences (.conf files).
 *
 * UNIX:    assigned to $HOME/.giFT
 * Windows: Searches for the first valid directory in:
 *          1. home_dir
 *          2. home_dir + /.giFT
 */
static char *local_dir = NULL;

/*****************************************************************************/

/* uname string formatted like an HTTP User Agent
 * TODO: this shouldn't be preformatted like that */
static char *gift_version = NULL;

/* list of all active children keyed by either thread ID or pid (windows and
 * linux respectively) */
static Dataset *active_children = NULL;

#ifdef WIN32
int socketpair (int family, int type, int protocol, int sv[2]);
#endif /* WIN32 */

/*****************************************************************************/

static void child_exit (int signum);

/*****************************************************************************/
/* PLATFORM_INIT and PLATFORM_CLEANUP */

/* accessors */
char *platform_data_dir   () { return data_dir;   }
char *platform_plugin_dir () { return plugin_dir; }
char *platform_local_dir  () { return local_dir;  }
char *platform_home_dir   () { return home_dir;   }

#ifndef WIN32

static int unix_init ()
{
	static char ph[PATH_MAX];
	char *h;

	/* attach signal handlers */
	signal (SIGPIPE, SIG_IGN);
	signal (SIGCHLD, child_exit);

	/* assign directory paths */
	data_dir   = STRDUP (DATA_DIR);
	plugin_dir = STRDUP (PLUGIN_DIR);

	/* determine the home directory */
	if ((h = getenv ("HOME")))
		home_dir = STRDUP (h);
	else
	{
		/* minor complaints, fallback to $USER */
		TRACE (("$HOME not set, attempting to compensate"));

		if ((h = getenv ("USER")))
		{
	 		snprintf (ph, sizeof (ph) - 1, "/home/%s", h);

			if (!access (ph, R_OK))
				home_dir = STRDUP (ph);
		}
	}

	if (!home_dir)
	{
		GIFT_FATAL (("unable to locate a sane home directory."));
		return FALSE;
	}

	local_dir = STRDUP (file_expand_path ("~/.giFT"));

	return TRUE;
}

#else /* WIN32 */

/* read registry and returns a string if the key was found in the registry
   and the key is the name of a valid directory */
static char *read_reg_dir (HKEY key_h, LPCTSTR key, LPCTSTR subkey)
{
	static char dir[MAX_PATH];
	DWORD len = sizeof (dir) - 1;
	DWORD key_type;

	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, key, 0,
	                   KEY_READ, &key_h) != ERROR_SUCCESS)
		return NULL;

	if (RegQueryValueEx (key_h, subkey, NULL, &key_type, dir,
						 &len) != ERROR_SUCCESS)
	{
		RegCloseKey (key_h);
		return NULL;
	}

	if (key_type != REG_SZ)
		return NULL;

	if (access (dir, R_OK))
		return NULL;

	return dir;
}

static char* wsa_error(int err, int major, int minor)
{
	int          i;
	static char  msg[256];
	static char *p;

	typedef struct
	{
		int   err;
		char* errmsg;
	} WSAStartupError;

	static WSAStartupError wsa_errors[] =
	{
		{ WSASYSNOTREADY,
		  "The underlying network subsystem is not ready for network "
		  "communication (Error %d)."},
		{ WSAVERNOTSUPPORTED,
		  "Version %d.%d of Winsock is not provided by this particular"
		  "Winsock implementation (Error %d)."},
		{ WSAEINPROGRESS,
		  "A blocking Winsock 1.1 operation is in progress (Error %d)."},
		{ WSAEPROCLIM,
		  "The limit on the number of tasks supported by the Winsock "
		  "implementation has been reached (Error %d)."},
		{ WSAEFAULT,
		  "The lpWSAData is not a valid pointer (Error %d)."}
	};

	p = msg;
	snprintf (p, sizeof (msg), "Unknown error %d", err);

	for (i = 0; i < sizeof (wsa_errors); i++)
	{
		if (err == wsa_errors[i].err)
			snprintf (p, sizeof (msg), wsa_errors[i].errmsg, major, minor,
					  err);
	}

	return p;
}

static int win32_init ()
{
	WSADATA wsa_data;
#ifdef DEBUG
	char    current_dir[_MAX_PATH] = {0};
	char    *home_tmp_dir;
#endif /* DEBUG */
	char    tmp[_MAX_PATH] = {0};
	char    *reg_dir;
	int     ret;

	/* initialize winsock2 library */
	if ((ret = WSAStartup (MAKEWORD (WS_MAJOR, WS_MINOR), &wsa_data)) != 0)
	{
		GIFT_ERROR ((wsa_error (ret, WS_MAJOR, WS_MINOR)));
		return FALSE;
	}

#ifdef DEBUG
	/* determine the current directory */
	GetCurrentDirectory (sizeof (current_dir), current_dir);
	if (current_dir)
		if (access (current_dir, R_OK))
			current_dir[0] = '\0';

	/* get the a HOME variable, if defined and accessable */
	home_tmp_dir = getenv ("HOME");
	if (home_tmp_dir)
		if (access (home_tmp_dir, R_OK))
			home_tmp_dir = NULL;
#endif /* DEBUG */

	/* determine the plugin directory */
	if ((reg_dir = read_reg_dir (HKEY_LOCAL_MACHINE, GIFT_REG_KEY,
		GIFT_REG_SUBKEY)))
		plugin_dir = STRDUP (reg_dir);
#ifdef DEBUG
	else if (current_dir)
		plugin_dir = STRDUP (current_dir);
#endif /* DEBUG */
	else
	{
		GIFT_ERROR (("Can't find giFT directory. Please reinstall giFT: %s",
					GIFT_STRERROR ()));
		return FALSE;
	}

	/* initialize data_dir */
	data_dir = malloc (strlen (plugin_dir) + 6); /* data/\0 */
	sprintf (data_dir, "%s/data", plugin_dir);
	if (access (data_dir, R_OK))
	{
		GIFT_ERROR (("Can't find %s directory. Please reinstall giFT.: %s",
					data_dir, GIFT_STRERROR ()));
		return FALSE;
	}

	/* determine the home directory */
	if ((reg_dir = read_reg_dir (HKEY_CURRENT_USER, GIFT_REG_KEY,
		GIFT_REG_SUBKEY)))
		home_dir = STRDUP (reg_dir);
#ifdef DEBUG
	else if (home_tmp_dir)
		home_dir = STRDUP (home_tmp_dir);
	else if (current_dir)
		home_dir = STRDUP (current_dir);
#endif /* DEBUG */
	else
	{
		GIFT_ERROR (("Can't find home directory. Please reinstall giFT."));
		return FALSE;
	}

	/* determine the local directory */
	snprintf (tmp, sizeof (tmp), "%s/gift.conf", home_dir);
	if (!access (tmp, R_OK))
		local_dir = STRDUP (home_dir);
	else
	{
		snprintf (tmp, sizeof (tmp), "%s/.giFT/gift.conf", home_dir);
		if (!access (tmp, R_OK))
		{
			local_dir = malloc (strlen (home_dir) + 7); /* .giFT/\0 */
			sprintf (local_dir, "%s/.giFT", home_dir);
		}
		else
		{
			GIFT_ERROR (("Can't find gift.conf Please reinstall giFT."));
			return FALSE;
		}
	}

	return TRUE;
}

#endif /* !WIN32 */

int platform_init ()
{
	int ret;

	assert (active_children == NULL);
	active_children = dataset_new (DATASET_HASH);

#ifndef WIN32
	ret = unix_init ();
#else /* WIN32 */
	ret = win32_init ();
#endif /* !WIN32 */

	if (!ret)
	{
		GIFT_FATAL (("Unable to initialize giFT.  Please see above errors"
		             "for more information."));
	}

	return ret;
}

#ifndef WIN32
static int kill_child (Dataset *d, DatasetNode *node, void *udata)
{
	pid_t pid = *((pid_t *)node->key);

	kill (pid, SIGTERM);

	return TRUE;
}

/* cleanup all active child processes
 * NOTE: submitted by soapy@gmx.de */
void unix_cleanup ()
{
	sigset_t sigset;
	sigset_t oldset;

	/* block signals to eliminate races accessing the list */
	sigfillset (&sigset);
	sigprocmask (SIG_BLOCK, &sigset, &oldset);

	/* deliver SIGTERM to all children */
	dataset_foreach (active_children, DATASET_FOREACH (kill_child), NULL);
	dataset_clear (active_children);

	/* unblock signals */
	sigprocmask (SIG_SETMASK, &oldset, NULL);
}
#endif /* !WIN32 */

void platform_cleanup ()
{
#ifndef WIN32
	unix_cleanup();
#else /* WIN32 */
	/* In a multithreaded environment, WSACleanup terminates Winsock operations
	 * for all threads. */
	if (WSACleanup () != 0)
	{
		GIFT_ERROR ((platform_net_error ()));
	}
#endif /* !WIN32 */
	free (data_dir);
	free (plugin_dir);
	free (home_dir);
	free (local_dir);
}

/*****************************************************************************/
/* PLATFORM_GETTIMEOFDAY */

void platform_gettimeofday (struct timeval *tv, void *unused)
{
#ifndef WIN32
	gettimeofday (tv, NULL);
#else /* WIN32 */
	struct _timeb tb;

	_ftime (&tb);

	tv->tv_sec  = tb.time;
	tv->tv_usec = tb.millitm * 1000;
#endif /* !WIN32 */
}

/*****************************************************************************/
/* PLATFORM_CHILD_PROC */

static void parent_wrapper (Protocol *p, Connection *c)
{
	SubprocessData *sdata;
	int             len;
	char            buf[RW_BUFFER];
	ParentFunc      p_func;

	sdata = c->data;
	p_func = sdata->pfunc;

	/* read the data waiting */
	len = recv (c->fd, buf, sizeof (buf) - 1, 0);

	/* call the parent function */
	if (p_func)
		(*p_func) (buf, len, sdata->udata);

	/* cleanup on EOF or error */
	if (len <= 0)
	{
		connection_close (c);
		free (sdata);
	}
}

static SUBPROCESS (child_wrapper)
{
	SubprocessData *sdata;

	if (!(sdata = param))
		return FALSE;

	/* call the child function */
	(*sdata->cfunc) (sdata);

	/* wake up the parent if its waiting */
	net_close (sdata->fd);

	/* free the data allocated previously */
	free (sdata);

	return TRUE;
}

/* create a child process
 *
 * UNIX    - fork ()
 * Windows - threads
 */
int platform_child_proc (ChildFunc c_func, ParentFunc p_func, void *udata)
{
	SubprocessData *pdata, *cdata;
	Connection     *c;
	int             pfd[2];
	pid_t           cpid;

#ifdef WIN32
	HANDLE hThread;
#endif /* WIN32 */

	if (!c_func)
		return FALSE;

	/* there needs to be some form of communication between child and parent,
	 * so we will establish it here */

	if (socketpair (AF_LOCAL, SOCK_STREAM, 0, pfd) == -1)
	{
		GIFT_ERROR (("socketpair: %s", platform_net_error()));
		return FALSE;
	}

	/* allocate the structure that will hold all the data we need to pass
	 * to the child/parent functions */
	if (!(pdata = malloc (sizeof (SubprocessData))))
		return FALSE;

	pdata->fd    = pfd[1];
	pdata->cfunc = c_func;
	pdata->pfunc = p_func;
	pdata->udata = udata;

#ifndef WIN32
	if ((cpid = fork ()) == -1)
	{
		GIFT_ERROR (("fork: %s", GIFT_STRERROR ()));
		return FALSE;
	}

	/* child */
	if (cpid == 0)
	{
		/* it's safe to use the parent's data, fork copies it */
		cdata = pdata;

		/* child is not allowed to read this descriptor */
		close (pfd[0]);

# ifdef HAVE_NICE
		nice (10);
# endif /* HAVE_NICE */

		/* restore SIGINT/SIGTERM behaviour, as we have no
		 * event loop in the child */
		signal (SIGTERM, SIG_DFL);
		signal (SIGINT, SIG_DFL);

		child_wrapper (cdata);

		_exit (0);
	}

	/* parent is not allowed to write this descriptor */
	close (pfd[1]);

	/* remember the child's pid so we may dispose of it on exit if it
	 * is still alive */
	dataset_insert (&active_children, &cpid, sizeof (cpid), "pid_t", 0);
#else /* WIN32 */
	/* copy the parent's data to avoid concurrent access */
	if (!(cdata = malloc (sizeof (SubprocessData))))
	{
		free (pdata);
		return FALSE;
	}

	memcpy (cdata, pdata, sizeof (SubprocessData));

	hThread = (HANDLE) _beginthreadex (NULL, (unsigned int) NULL,
	                                   (ChildFunc) child_wrapper, cdata, 0U,
	                                   (unsigned int *) &cpid);

	if (hThread == (HANDLE) -1 || hThread == (HANDLE) 0)
	{
		GIFT_ERROR (("_beginthreadex: %s", GIFT_STRERROR ()));
		return FALSE;
	}

	/* So the system doesn't grind to a halt */
	SetThreadPriority (hThread, THREAD_PRIORITY_LOWEST);

	/* so we can look up the thread handle by the thread identifier why there
	 * is an ID *and* a handle is beyond me */
	dataset_insert (&active_children, &cpid, sizeof (cpid), hThread, 0);
#endif /* !WIN32 */

	/* we are now the parent, start selecting the socket */
	c = connection_new (NULL);
	c->fd   = pfd[0];
	c->data = pdata;

	input_add (NULL, c, INPUT_READ, parent_wrapper, FALSE);

	return TRUE;
}

#ifndef WIN32
static void child_exit (int signum)
{
	waitpid (-1, NULL, WUNTRACED | WNOHANG);

#if 0
	pid_t    pid;
	sigset_t sigset;
	sigset_t oldset;

	/* block signals to eliminate races when accessing the list */
	sigfillset (&sigset);
	sigprocmask (SIG_BLOCK, &sigset, &oldset);

	/* exorcise zombies and remove their pids from our list */
	while ((pid = waitpid (-1, NULL, WUNTRACED | WNOHANG)) > 0)
		dataset_remove (active_children, &pid, sizeof (pid));

	if (pid == -1)
		GIFT_ERROR (("waitpid: %s", GIFT_STRERROR ()));

	/* unblock signals */
	sigprocmask (SIG_SETMASK, &oldset, NULL);
#endif
}
#endif /* !WIN32 */

/*****************************************************************************/
/* PLATFORM_VERSION */

/* determine the giFT version string (includes platform specific data) */
char *platform_version ()
{
	/* gift_version does not exist, so calculate it once */
	if (!gift_version)
	{
		size_t alloc_len;
		size_t len;
#ifndef WIN32
		struct utsname os;
#else /* WIN32 */
		OSVERSIONINFO vinfo;
		char         *win_str = "Windows";
#endif /* !WIN32 */

		alloc_len = 2 + strlen (PACKAGE) + strlen (VERSION);

		if (!(gift_version = malloc (alloc_len)))
			return NULL;

		len = snprintf (gift_version, alloc_len, "%s/%s",
		                PACKAGE, VERSION);

		/* if we have any extra OS-specific data to use, add it here
		 * NOTE: realloc'ing is less efficient, of course.  However, this is
		 * much easier to manipulate the above version string, as well as
		 * allowing us to return _something_ even if the OS fails to
		 * determine itself for whatever reason */
#ifndef WIN32
		if (uname (&os) == -1)
			return gift_version;

		alloc_len += 32;
		alloc_len += strlen (os.sysname);
		alloc_len += strlen (os.release);
		alloc_len += strlen (os.machine);

		if (!(gift_version = realloc (gift_version, alloc_len)))
			return NULL;

		len += snprintf (gift_version + len, alloc_len - len, " (%s %s %s)",
		                 os.sysname, os.release, os.machine);
#else /* WIN32 */
		vinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

		if (!GetVersionEx (&vinfo))
			return gift_version;

		/* isn't the Windows API so pretty by comparison? */
		switch (vinfo.dwPlatformId)
		{
		 case VER_PLATFORM_WIN32_WINDOWS:
			if (vinfo.dwMajorVersion == 4 && vinfo.dwMinorVersion == 0)
				win_str = "95";
			else if (vinfo.dwMajorVersion == 4 && vinfo.dwMinorVersion == 90)
				win_str = "ME";
			else
				win_str = "98";
			break;
		 case VER_PLATFORM_WIN32_NT:
			if (vinfo.dwMajorVersion <= 4)
				win_str = "NT";
			else if (vinfo.dwMajorVersion == 5 && vinfo.dwMinorVersion == 0)
				win_str = "2000";
			else if (vinfo.dwMajorVersion == 5 && vinfo.dwMinorVersion == 1)
				win_str = "XP";
			break;
		}

		alloc_len += 64;
		alloc_len += strlen (win_str);
		alloc_len += strlen (vinfo.szCSDVersion);

		if (!(gift_version = realloc (gift_version, alloc_len)))
			return NULL;

		len += snprintf (gift_version + len, alloc_len - len,
		                 " (Windows %s %d.%d.%d %s)",
		                 win_str, vinfo.dwMajorVersion, vinfo.dwMinorVersion,
		                 vinfo.dwBuildNumber && 0xffff, vinfo.szCSDVersion);
#endif /* !WIN32 */
	}

	return gift_version;
}

/*****************************************************************************/
/* PLATFORM ERROR HANDLERS */

#ifdef WIN32
char *win32_format_message (const DWORD err)
{
	static char buf[2048];
	int         ret;

	/* just keep telling yourself -- Microsoft will be gone soon */
	ret =
	    FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                   NULL, errno, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
	                   (LPTSTR) &buf, sizeof (buf), NULL);

	if (ret == 0)
		snprintf (buf, sizeof(buf), "Unknown Windows error %d", err);

	return buf;
}
#endif /* WIN32 */

unsigned long platform_errno ()
{
#ifndef WIN32
	return (unsigned long) errno;
#else /* WIN32 */
	return (unsigned long) GetLastError ();
#endif /* !WIN32 */
}


char *platform_error ()
{
#ifndef WIN32
	return strerror (errno);
#else /* WIN32 */
	return win32_format_message (GetLastError ());
#endif /* !WIN32 */
}

unsigned long platform_net_errno ()
{
#ifndef WIN32
	return (unsigned long) errno;
#else /* WIN32 */
	/* F*cking windows has a different error function for socket functions. */
	return (unsigned long) WSAGetLastError ();
#endif /* !WIN32 */
}

char *platform_net_error ()
{
#ifndef WIN32
	return strerror (errno);
#else /* WIN32 */
	/* F*cking windows has a different error function for socket functions. */
	return win32_format_message (WSAGetLastError ());
#endif /* !WIN32 */
}

/*****************************************************************************/
/* socketpair function missing (of course) on windows */

#ifdef WIN32

#define PF_LOCAL AF_LOCAL

static int socketpair (int family, int type, int protocol, int sv[2])
{
	int insock, outsock, newsock;
	struct sockaddr_in sock_in, sock_out;
	int len;

	/* windowz only has AF_INET (we use that for AF_LOCAL too) */
	if (family != AF_LOCAL && family != AF_INET)
		return -1;

	/* STRAM and DGRAM sockets only */
	if (type != SOCK_STREAM && type != SOCK_DGRAM)
		return -1;

	/* yes, we all love windoze */
	if ((family == AF_LOCAL && protocol != PF_UNSPEC && protocol != PF_LOCAL)
	    || (family == AF_INET && protocol != PF_UNSPEC && protocol != PF_INET))
		return -1;


	/* create the first socket */
	newsock = socket (AF_INET, type, 0);
	if (newsock == INVALID_SOCKET)
	{
		TRACE (("first socket call failed"));
		return -1;
	}

	/* bind the socket to any unused port */
	sock_in.sin_family      = AF_INET;
	sock_in.sin_port        = 0;
	sock_in.sin_addr.s_addr = INADDR_ANY;
	if (bind (newsock, (struct sockaddr *) &sock_in, sizeof (sock_in)) < 0)
	{
		TRACE (("bind failed"));
		closesocket (newsock);
		return -1;
	}
	len = sizeof (sock_in);
	if (getsockname (newsock, (struct sockaddr *) &sock_in, &len) < 0)
	{
		TRACE (("getsockname error"));
		closesocket (newsock);
		return -1;
	}

	/* For stream sockets, create a listener */
	if (type == SOCK_STREAM)
		listen (newsock, 2);

	/* create a connecting socket */
	outsock = socket (AF_INET, type, 0);
	if (outsock == INVALID_SOCKET)
	{
		TRACE (("second socket call failed"));
		closesocket (newsock);
		return -1;
	}

	/* For datagram sockets, bind the 2nd socket to an unused address, too */
	if (type == SOCK_DGRAM)
	{
		sock_out.sin_family       = AF_INET;
		sock_out.sin_port        = 0;
		sock_out.sin_addr.s_addr = INADDR_ANY;
		if (bind (outsock, (struct sockaddr *) &sock_out, sizeof (sock_out)) < 0)
		{
			TRACE (("bind failed"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
		len = sizeof (sock_out);
		if (getsockname (outsock, (struct sockaddr *) &sock_out, &len) < 0)
		{
			TRACE (("getsockname error"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
	}

	/* Force IP address to loopback */
	sock_in.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	if (type == SOCK_DGRAM)
		sock_out.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

	/* Do a connect */
	if (connect (outsock, (struct sockaddr *) &sock_in, sizeof (sock_in)) < 0)
	{
		TRACE (("connect error"));
		closesocket (newsock);
		closesocket (outsock);
		return -1;
	}

	if (type == SOCK_STREAM)
	{
		/* For stream sockets, accept the connection and close the listener */
		len = sizeof (sock_in);
		insock = accept (newsock, (struct sockaddr *) &sock_in, &len);
		if (insock == INVALID_SOCKET)
		{
			TRACE (("accept error"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
		closesocket (newsock);
	}
	else
	{
		/* For datagram sockets, connect the 2nd socket */
		if (connect (newsock, (struct sockaddr *) &sock_out, sizeof (sock_out)) < 0)
		{
			TRACE (("connect error"));
			closesocket (newsock);
			closesocket (outsock);
			return -1;
		}
		insock = newsock;
	}

	/* set the descriptors */
	sv[0] = insock;
	sv[1] = outsock;

	/* we've done it */
	return 0;
}

#endif /* WIN32 */
