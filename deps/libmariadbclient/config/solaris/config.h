/* Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MY_CONFIG_H
#define MY_CONFIG_H
#define DOT_FRM_VERSION 6
/* Headers we may want to use. */
#define STDC_HEADERS 1
/* #undef _GNU_SOURCE */
#define HAVE_ALLOCA_H 1
#define HAVE_AIO_H 1
#define HAVE_ARPA_INET_H 1
/* #undef HAVE_ASM_MSR_H */
/* #undef HAVE_ASM_TERMBITS_H */
#define HAVE_BSEARCH 1
#define HAVE_CRYPT_H 1
#define HAVE_CURSES_H 1
#define HAVE_CXXABI_H 1
/* #undef HAVE_BFD_H */
/* #undef HAVE_NCURSES_H */
/* #undef HAVE_NDIR_H */
#define HAVE_DIRENT_H 1
#define HAVE_DLFCN_H 1
#define HAVE_EXECINFO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FENV_H 1
#define HAVE_FLOAT_H 1
#define HAVE_FLOATINGPOINT_H 1
#define HAVE_FNMATCH_H 1
/* #undef HAVE_FPU_CONTROL_H */
#define HAVE_GRP_H 1
#define HAVE_EXPLICIT_TEMPLATE_INSTANTIATION 1
/* #undef HAVE_IA64INTRIN_H */
#define HAVE_IEEEFP_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MALLOC_H 1
#define HAVE_MEMORY_H 1
#define HAVE_NETINET_IN_H 1
/* #undef HAVE_PATHS_H */
#define HAVE_POLL_H 1
#define HAVE_PORT_H 1
#define HAVE_PWD_H 1
#define HAVE_SCHED_H 1
/* #undef HAVE_SELECT_H */
#define HAVE_SOLARIS_LARGE_PAGES 1
#define HAVE_STDDEF_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STDINT_H 1
#define HAVE_SEMAPHORE_H 1
#define HAVE_SYNCH_H 1
/* #undef HAVE_SYSENT_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_CDEFS_H */
#define HAVE_SYS_FILE_H 1
/* #undef HAVE_SYS_FPU_H */
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_IPC_H 1
/* #undef HAVE_SYS_MALLOC_H */
#define HAVE_SYS_MMAN_H 1
/* #undef HAVE_SYS_NDIR_H */
#define HAVE_SYS_PTE_H 1
#define HAVE_SYS_PTEM_H 1
/* #undef HAVE_SYS_PRCTL_H */
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SHM_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_SOCKIO_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_STREAM_H 1
/* #undef HAVE_SYS_TERMCAP_H */
/* #undef HAVE_SYS_TIMEB_H */
#define HAVE_SYS_TIMES_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UN_H 1
/* #undef HAVE_SYS_VADVISE_H */
#define HAVE_TERM_H 1
/* #undef HAVE_TERMBITS_H */
#define HAVE_TERMIOS_H 1
#define HAVE_TERMIO_H 1
/* #undef HAVE_TERMCAP_H */
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_UTIME_H 1
/* #undef HAVE_VARARGS_H */
/* #undef HAVE_VIS_H */
#define HAVE_SYS_UTIME_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_SYS_PARAM_H 1

/* Libraries */
/* #undef HAVE_LIBPTHREAD */
#define HAVE_LIBM 1
/* #undef HAVE_LIBDL */
/* #undef HAVE_LIBRT */
#define HAVE_LIBSOCKET 1
#define HAVE_LIBNSL 1
/* #undef HAVE_LIBCRYPT */
/* #undef HAVE_LIBMTMALLOC */
/* #undef HAVE_LIBWRAP */
/* Does "struct timespec" have a "sec" and "nsec" field? */
/* #undef HAVE_TIMESPEC_TS_SEC */

/* Readline */
/* #undef HAVE_HIST_ENTRY */
/* #undef USE_LIBEDIT_INTERFACE */
#define USE_NEW_READLINE_INTERFACE 1

/* #undef FIONREAD_IN_SYS_IOCTL */
/* #undef GWINSZ_IN_SYS_IOCTL */
/* #undef TIOCSTAT_IN_SYS_IOCTL */
#define FIONREAD_IN_SYS_FILIO 1

/* Functions we may want to use. */
/* #undef HAVE_AIOWAIT */
#define HAVE_ALARM 1
#define HAVE_ALLOCA 1
/* #undef HAVE_BFILL */
/* #undef HAVE_BMOVE */
#define HAVE_BZERO 1
#define HAVE_INDEX 1
#define HAVE_CHOWN 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_CRYPT 1
#define HAVE_CUSERID 1
#define HAVE_CXX_NEW 1
#define HAVE_DIRECTIO 1
#define HAVE_DLERROR 1
#define HAVE_DLOPEN 1
#define HAVE_DOPRNT 1
#define HAVE_FCHMOD 1
#define HAVE_FCNTL 1
#define HAVE_FCONVERT 1
#define HAVE_FDATASYNC 1
#define HAVE_DECL_FDATASYNC 1
#define HAVE_FESETROUND 1
#define HAVE_FINITE 1
#define HAVE_FP_EXCEPT 1
#define HAVE_FPSETMASK 1
#define HAVE_FSEEKO 1
#define HAVE_FSYNC 1
#define HAVE_FTIME 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETCWD 1
#define HAVE_GETHOSTBYADDR_R 1
#define HAVE_GETHRTIME 1
#define HAVE_GETLINE 1
#define HAVE_GETNAMEINFO 1
#define HAVE_GETPAGESIZE 1
#define HAVE_GETPASS 1
#define HAVE_GETPASSPHRASE 1
#define HAVE_GETPWNAM 1
#define HAVE_GETPWUID 1
#define HAVE_GETRLIMIT 1
#define HAVE_GETRUSAGE 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GETWD 1
#define HAVE_GMTIME_R 1
/* #undef gmtime_r */
#define HAVE_INITGROUPS 1
#define HAVE_ISSETUGID 1
#define HAVE_GETUID 1
#define HAVE_GETEUID 1
#define HAVE_GETGID 1
#define HAVE_GETEGID 1
#define HAVE_ISNAN 1
/* #undef HAVE_ISINF */
#define HAVE_LARGE_PAGE_OPTION 1
#define HAVE_LDIV 1
#define HAVE_LRAND48 1
#define HAVE_LOCALTIME_R 1
#define HAVE_LOG2 1
#define HAVE_LONGJMP 1
#define HAVE_LSTAT 1
#define HAVE_MEMALIGN 1
/* #define HAVE_MLOCK 1 see Bug#54662 */
/* #undef HAVE_NPTL */
#define HAVE_NL_LANGINFO 1
#define HAVE_MADVISE 1
#define HAVE_DECL_MADVISE 1
/* #undef HAVE_DECL_TGOTO */
#define HAVE_DECL_MHA_MAPSIZE_VA
/* #undef HAVE_MALLINFO */
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MKSTEMP 1
#define HAVE_MLOCKALL 1
#define HAVE_MMAP 1
#define HAVE_MMAP64 1
#define HAVE_PERROR 1
#define HAVE_POLL 1
#define HAVE_PORT_CREATE 1
#define HAVE_POSIX_FALLOCATE 1
#define HAVE_PREAD 1
#define HAVE_PAUSE_INSTRUCTION 1
/* #undef HAVE_FAKE_PAUSE_INSTRUCTION */
/* #undef HAVE_RDTSCLL */
/* #undef HAVE_READ_REAL_TIME */
/* #undef HAVE_PTHREAD_ATTR_CREATE */
#define HAVE_PTHREAD_ATTR_GETSTACKSIZE 1
/* #undef HAVE_PTHREAD_ATTR_SETPRIO */
/* #undef HAVE_PTHREAD_ATTR_SETSCHEDPARAM */
#define HAVE_PTHREAD_ATTR_SETSCOPE 1
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1
/* #undef HAVE_PTHREAD_CONDATTR_CREATE */
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#define HAVE_PTHREAD_KEY_DELETE 1
#define HAVE_PTHREAD_KEY_DELETE 1
/* #undef HAVE_PTHREAD_KILL */
#define HAVE_PTHREAD_RWLOCK_RDLOCK 1
/* #undef HAVE_PTHREAD_SETPRIO_NP */
/* #undef HAVE_PTHREAD_SETSCHEDPARAM */
#define HAVE_PTHREAD_SIGMASK 1
/* #undef HAVE_PTHREAD_THREADMASK */
/* #undef HAVE_PTHREAD_YIELD_NP */
/* #undef HAVE_PTHREAD_YIELD_ZERO_ARG */
#define PTHREAD_ONCE_INITIALIZER PTHREAD_ONCE_INIT
#define HAVE_PUTENV 1
#define HAVE_RE_COMP 1
#define HAVE_REGCOMP 1
#define HAVE_READDIR_R 1
#define HAVE_READLINK 1
#define HAVE_REALPATH 1
#define HAVE_RENAME 1
#define HAVE_RINT 1
#define HAVE_RWLOCK_INIT 1
#define HAVE_SCHED_YIELD 1
#define HAVE_SELECT 1
/* #undef HAVE_SETFD */
#define HAVE_SETENV 1
#define HAVE_SETLOCALE 1
#define HAVE_SIGADDSET 1
#define HAVE_SIGEMPTYSET 1
#define HAVE_SIGHOLD 1
#define HAVE_SIGSET 1
#define HAVE_SIGSET_T 1
#define HAVE_SIGACTION 1
/* #undef HAVE_SIGTHREADMASK */
#define HAVE_SIGWAIT 1
#define HAVE_SLEEP 1
#define HAVE_SNPRINTF 1
#define HAVE_STPCPY 1
#define HAVE_STRERROR 1
#define HAVE_STRCOLL 1
#define HAVE_STRSIGNAL 1
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
/* #undef HAVE_FGETLN */
#define HAVE_STRNLEN 1
#define HAVE_STRPBRK 1
/* #undef HAVE_STRSEP */
#define HAVE_STRSTR 1
#define HAVE_STRTOK_R 1
#define HAVE_STRTOL 1
#define HAVE_STRTOLL 1
#define HAVE_STRTOUL 1
#define HAVE_STRTOULL 1
#define HAVE_SHMAT 1
#define HAVE_SHMCTL 1
#define HAVE_SHMDT 1
#define HAVE_SHMGET 1
#define HAVE_TELL 1
#define HAVE_TEMPNAM 1
#define HAVE_THR_SETCONCURRENCY 1
#define HAVE_THR_YIELD 1
#define HAVE_TIME 1
#define HAVE_TIMES 1
#define HAVE_VALLOC 1
#define HAVE_VIO_READ_BUFF 1
#define HAVE_VASPRINTF 1
#define HAVE_VPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_FTRUNCATE 1
#define HAVE_TZNAME 1
#define HAVE_AIO_READ 1
/* Symbols we may use */
/* #undef HAVE_SYS_ERRLIST */
/* used by stacktrace functions */
/* #undef HAVE_BSS_START */
#define HAVE_BACKTRACE 1
#define HAVE_BACKTRACE_SYMBOLS 1
#define HAVE_BACKTRACE_SYMBOLS_FD 1
#define HAVE_PRINTSTACK 1
#define HAVE_STRUCT_SOCKADDR_IN6 1
#define HAVE_STRUCT_IN6_ADDR 1
/* #undef HAVE_NETINET_IN6_H */
#define HAVE_IPV6 1
/* #undef ss_family */
/* #undef HAVE_SOCKADDR_IN_SIN_LEN */
/* #undef HAVE_SOCKADDR_IN6_SIN6_LEN */
/* #undef HAVE_TIMESPEC_TS_SEC */
#define STRUCT_DIRENT_HAS_D_INO 1
/* #undef STRUCT_DIRENT_HAS_D_NAMLEN */
#define SPRINTF_RETURNS_INT 1

#define USE_MB 1
#define USE_MB_IDENT 1

/* this means that valgrind headers and macros are available */
/* #undef HAVE_VALGRIND */

/* this means WITH_VALGRIND - we change some code paths for valgrind */
/* #undef HAVE_valgrind */

/* Types we may use */
#ifdef __APPLE__
  /*
    Special handling required for OSX to support universal binaries that 
    mix 32 and 64 bit architectures.
  */
  #if(__LP64__)
    #define SIZEOF_LONG 8
  #else
    #define SIZEOF_LONG 4
  #endif
  #define SIZEOF_VOIDP   SIZEOF_LONG
  #define SIZEOF_CHARP   SIZEOF_LONG
  #define SIZEOF_SIZE_T  SIZEOF_LONG
#else
/* No indentation, to fetch the lines from verification scripts */
#define SIZEOF_LONG   4
#define SIZEOF_VOIDP  4
#define SIZEOF_CHARP  4
#define SIZEOF_SIZE_T 4
#endif

#define SIZEOF_CHAR 1
#define HAVE_CHAR 1
#define HAVE_LONG 1
#define HAVE_CHARP 1
#define SIZEOF_SHORT 2
#define HAVE_SHORT 1
#define SIZEOF_INT 4
#define HAVE_INT 1
#define SIZEOF_LONG_LONG 8
#define HAVE_LONG_LONG 1
#define SIZEOF_OFF_T 8
#define HAVE_OFF_T 1
#define SIZEOF_SIGSET_T 16
#define HAVE_SIGSET_T 1
#define HAVE_SIZE_T 1
/* #undef SIZEOF_UCHAR */
/* #undef HAVE_UCHAR */
#define SIZEOF_UINT 4
#define HAVE_UINT 1
#define SIZEOF_ULONG 4
#define HAVE_ULONG 1
/* #undef SIZEOF_INT8 */
/* #undef HAVE_INT8 */
/* #undef SIZEOF_UINT8 */
/* #undef HAVE_UINT8 */
/* #undef SIZEOF_INT16 */
/* #undef HAVE_INT16 */
/* #undef SIZEOF_UINT16 */
/* #undef HAVE_UINT16 */
/* #undef SIZEOF_INT32 */
/* #undef HAVE_INT32 */
/* #undef SIZEOF_UINT32 */
/* #undef HAVE_UINT32 */
/* #undef SIZEOF_U_INT32_T */
/* #undef HAVE_U_INT32_T */
/* #undef SIZEOF_INT64 */
/* #undef HAVE_INT64 */
/* #undef SIZEOF_UINT64 */
/* #undef HAVE_UINT64 */
/* #undef SIZEOF_BOOL */
/* #undef HAVE_BOOL */

#define SOCKET_SIZE_TYPE socklen_t

#define HAVE_MBSTATE_T

#define MAX_INDEXES 64

#define QSORT_TYPE_IS_VOID 1
#define RETQSORTTYPE void

#define SIGNAL_RETURN_TYPE_IS_VOID 1
#define RETSIGTYPE void
#define VOID_SIGHANDLER 1
#define STRUCT_RLIMIT struct rlimit

#ifdef __APPLE__
  #if __BIG_ENDIAN
    #define WORDS_BIGENDIAN 1
  #endif
#else
/* #undef WORDS_BIGENDIAN */
#endif

/* Define to `__inline__' or `__inline' if that's what the C compiler calls
   it, or to nothing if 'inline' is not supported under any name.  */
#define C_HAS_inline 1
#if !(C_HAS_inline)
#ifndef __cplusplus
# define inline 
#endif
#endif


/* #undef TARGET_OS_LINUX */

#define HAVE_WCTYPE_H 1
#define HAVE_WCHAR_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_MBRLEN
/* #undef HAVE_MBSCMP */
#define HAVE_MBSRTOWCS
#define HAVE_WCRTOMB
#define HAVE_MBRTOWC
#define HAVE_WCSCOLL
#define HAVE_WCSDUP
#define HAVE_WCWIDTH
#define HAVE_WCTYPE
#define HAVE_ISWLOWER 1
#define HAVE_ISWUPPER 1
#define HAVE_TOWLOWER 1
#define HAVE_TOWUPPER 1
#define HAVE_ISWCTYPE 1
#define HAVE_WCHAR_T 1
#define HAVE_WCTYPE_T 1
#define HAVE_WINT_T 1


#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_LANGINFO_CODESET 
#define HAVE_TCGETATTR 1
#define HAVE_FLOCKFILE 1

#define HAVE_WEAK_SYMBOL 1
#define HAVE_ABI_CXA_DEMANGLE 1


#define HAVE_POSIX_SIGNALS 1
/* #undef HAVE_BSD_SIGNALS */
/* #undef HAVE_SVR3_SIGNALS */
/* #undef HAVE_V7_SIGNALS */


#define HAVE_SOLARIS_STYLE_GETHOST 1

/* #undef MY_ATOMIC_MODE_DUMMY */
/* #undef MY_ATOMIC_MODE_RWLOCKS */
/* #undef HAVE_GCC_ATOMIC_BUILTINS */
#define HAVE_SOLARIS_ATOMIC 1
/* #undef HAVE_DECL_SHM_HUGETLB */
/* #undef HAVE_LARGE_PAGES */
/* #undef HUGETLB_USE_PROC_MEMINFO */
/* #undef NO_FCNTL_NONBLOCK */
#define NO_ALARM 1

/* #undef _LARGE_FILES */
#define _LARGEFILE_SOURCE 1
/* #undef _LARGEFILE64_SOURCE */
#define _FILE_OFFSET_BITS 64

#define TIME_WITH_SYS_TIME 1

#define STACK_DIRECTION -1

#define SYSTEM_TYPE "solaris11"
#define MACHINE_TYPE "i386"
/* #undef HAVE_DTRACE */

#define SIGNAL_WITH_VIO_CLOSE 1

/* Windows stuff, mostly functions, that have Posix analogs but named differently */
/* #undef S_IROTH */
/* #undef S_IFIFO */
/* #undef IPPROTO_IPV6 */
/* #undef IPV6_V6ONLY */
/* #undef sigset_t */
/* #undef mode_t */
/* #undef SIGQUIT */
/* #undef SIGPIPE */
/* #undef isnan */
/* #undef finite */
/* #undef popen */
/* #undef pclose */
/* #undef ssize_t */
/* #undef strcasecmp */
/* #undef strncasecmp */
/* #undef snprintf */
/* #undef strtok_r */
/* #undef strtoll */
/* #undef strtoull */
/* #undef vsnprintf */
#if (_MSC_VER > 1310)
# define HAVE_SETENV
#define setenv(a,b,c) _putenv_s(a,b)
#endif
#define PSAPI_VERSION 1     /* for GetProcessMemoryInfo() */

/*
  MySQL features
*/
/* #undef ENABLED_LOCAL_INFILE */
#define ENABLED_PROFILING 1
/* #undef EXTRA_DEBUG */
/* #undef BACKUP_TEST */
/* #undef CYBOZU */
/* #undef USE_SYMDIR */

/* Character sets and collations */
#define MYSQL_DEFAULT_CHARSET_NAME "latin1"
#define MYSQL_DEFAULT_COLLATION_NAME "latin1_swedish_ci"

#define USE_MB 1
#define USE_MB_IDENT 1
/* #undef USE_STRCOLL */

/* This should mean case insensitive file system */
/* #undef FN_NO_CASE_SENSE */

#define HAVE_CHARSET_armscii8 1
#define HAVE_CHARSET_ascii 1
#define HAVE_CHARSET_big5 1
#define HAVE_CHARSET_cp1250 1
#define HAVE_CHARSET_cp1251 1
#define HAVE_CHARSET_cp1256 1
#define HAVE_CHARSET_cp1257 1
#define HAVE_CHARSET_cp850 1
#define HAVE_CHARSET_cp852 1 
#define HAVE_CHARSET_cp866 1
#define HAVE_CHARSET_cp932 1
#define HAVE_CHARSET_dec8 1
#define HAVE_CHARSET_eucjpms 1
#define HAVE_CHARSET_euckr 1
#define HAVE_CHARSET_gb2312 1
#define HAVE_CHARSET_gbk 1
#define HAVE_CHARSET_geostd8 1
#define HAVE_CHARSET_greek 1
#define HAVE_CHARSET_hebrew 1
#define HAVE_CHARSET_hp8 1
#define HAVE_CHARSET_keybcs2 1
#define HAVE_CHARSET_koi8r 1
#define HAVE_CHARSET_koi8u 1
#define HAVE_CHARSET_latin1 1
#define HAVE_CHARSET_latin2 1
#define HAVE_CHARSET_latin5 1
#define HAVE_CHARSET_latin7 1
#define HAVE_CHARSET_macce 1
#define HAVE_CHARSET_macroman 1
#define HAVE_CHARSET_sjis 1
#define HAVE_CHARSET_swe7 1
#define HAVE_CHARSET_tis620 1
#define HAVE_CHARSET_ucs2 1
#define HAVE_CHARSET_ujis 1
#define HAVE_CHARSET_utf8mb4 1
/* #undef HAVE_CHARSET_utf8mb3 */
#define HAVE_CHARSET_utf8 1
#define HAVE_CHARSET_utf16 1
#define HAVE_CHARSET_utf32 1
#define HAVE_UCA_COLLATIONS 1
#define HAVE_COMPRESS 1


/*
  Stuff that always need to be defined (compile breaks without it)
*/
#define HAVE_SPATIAL 1
#define HAVE_RTREE_KEYS 1
#define HAVE_QUERY_CACHE 1
#define BIG_TABLES 1

#define DEFAULT_MYSQL_HOME "/usr/local/mysql"
#define SHAREDIR "/usr/local/mysql/share"
#define DEFAULT_BASEDIR "/usr/local/mysql"
#define MYSQL_DATADIR "/usr/local/mysql/data"
#define DEFAULT_CHARSET_HOME "/usr/local/mysql"
#define PLUGINDIR "/usr/local/mysql/lib/plugin"
/* #undef DEFAULT_SYSCONFDIR */

/* #undef SO_EXT */

#define MYSQL_VERSION_MAJOR 10
#define MYSQL_VERSION_MINOR 1
#define MYSQL_VERSION_PATCH 8
#define MYSQL_VERSION_EXTRA ""

#define PACKAGE "mysql"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "MySQL Server"
#define PACKAGE_STRING "MySQL Server 10.1.8"
#define PACKAGE_TARNAME "mysql"
#define PACKAGE_VERSION "10.1.8"
#define VERSION "10.1.8"
#define PROTOCOL_VERSION 10



/* time_t related defines */

#define SIZEOF_TIME_T 4
/* #undef TIME_T_UNSIGNED */

#endif
