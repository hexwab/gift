#ifndef _WCWIDTH_H
#define _WCWIDTH_H

/* These are the functions from wcwidth.c that we want to use. */
int mk_wcwidth(wchar_t ucs);
int mk_wcswidth(const wchar_t *pwcs, size_t n);

#endif
