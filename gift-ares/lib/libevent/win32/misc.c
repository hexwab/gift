#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <sys/timeb.h>
#include <time.h>

/****************************************************************************
 *
 * Function: gettimeofday(struct timeval *, struct timezone *)
 *
 * Purpose:  Get current time of day.
 *
 * Arguments: tv => Place to store the curent time of day.
 *            tz => Ignored.
 *
 * Returns: 0 => Success.
 *
 ****************************************************************************/

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  struct _timeb tb;

	if(tv == NULL)
		return -1;

	_ftime(&tb);
	tv->tv_sec = tb.time;
	tv->tv_usec = ((int) tb.millitm) * 1000;
	return 0;
}

int
win_read(int fd, void *buf, unsigned int length)
{
	DWORD dwBytesRead;
	int res = ReadFile((HANDLE) fd, buf, length, &dwBytesRead, NULL);
	if (res == 0) {
		DWORD error = GetLastError();
		if (error == ERROR_NO_DATA)
			return (0);
		return (-1);
	} else
		return (dwBytesRead);
}

int
win_write(int fd, void *buf, unsigned int length)
{
	DWORD dwBytesWritten;
	int res = WriteFile((HANDLE) fd, buf, length, &dwBytesWritten, NULL);
	if (res == 0) {
		DWORD error = GetLastError();
		if (error == ERROR_NO_DATA)
			return (0);
		return (-1);
	} else
		return (dwBytesWritten);
}

