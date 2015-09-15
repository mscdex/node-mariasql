// XXX: added by mscdex
// MSVS prior to version 2015 do not have a proper snprintf implementation
#if defined(_MSC_VER) && _MSC_VER < 1900
# define snprintf c99_snprintf
# define vsnprintf c99_vsnprintf
#endif
