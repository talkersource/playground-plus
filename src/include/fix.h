/*
 * Playground+ - fix.h
 * Code to fix all those nasty warnings
 * ---------------------------------------------------------------------------
*/

#ifndef SOLARIS
struct rlimit;
struct timval;
struct itimerval;

#ifdef NEED_SIGPAUSE_DECL
extern int      sigpause(int);
#endif /* NEED_SIGPAUSE_DECL */

#ifndef BSDISH
   #if !defined(linux)
      extern int      setrlimit(int, struct rlimit *);
   #endif /* LINUX */
#endif

#ifdef NEED_GETITIMER_DECL
extern int      getitimer(int, struct itimerval *);
#endif /* NEED_GETITIMER_DECL */
#ifdef NEED_GETRLIMIT_DECL
extern int      getrlimit(int, struct rlimit *);
#endif /* NEED_GETRLIMIT_DECL */

#ifndef BSDISH
   #if !defined(linux)
      extern int      setitimer(int, struct itimerval *, struct itimerval *);
   #endif /* LINUX */
   extern void     lower_case(char *);
   #if !defined(linux)
      extern int      strcasecmp(char *, char *);
   #endif /* LINUX */
#endif /* BSDISH */

extern void     lower_case(char *);
#endif
