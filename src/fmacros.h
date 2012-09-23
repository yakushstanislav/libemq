#ifndef _EMQ_MACROS_H_
#define _EMQ_MACROS_H_

#if !defined(_BSD_SOURCE)
	#define _BSD_SOURCE
#endif

#if defined(__linux__)
	#define _XOPEN_SOURCE 600
#else
	#define _XOPEN_SOURCE
#endif

#endif
