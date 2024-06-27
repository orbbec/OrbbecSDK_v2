
#ifndef OB_EXPORT_H
#define OB_EXPORT_H

#ifdef OB_STATIC_DEFINE
#  define OB_EXPORT
#  define OB_NO_EXPORT
#else
#  ifndef OB_EXPORT
#    ifdef openobsdk_EXPORTS
        /* We are building this library */
#      define OB_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define OB_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef OB_NO_EXPORT
#    define OB_NO_EXPORT 
#  endif
#endif

#ifndef OB_DEPRECATED
#  define OB_DEPRECATED __declspec(deprecated)
#endif

#ifndef OB_DEPRECATED_EXPORT
#  define OB_DEPRECATED_EXPORT OB_EXPORT OB_DEPRECATED
#endif

#ifndef OB_DEPRECATED_NO_EXPORT
#  define OB_DEPRECATED_NO_EXPORT OB_NO_EXPORT OB_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef OB_NO_DEPRECATED
#    define OB_NO_DEPRECATED
#  endif
#endif

#endif /* OB_EXPORT_H */