/*

    WGL loader generated by glad 0.1.13a0 on Wed Mar 22 20:19:36 2017.

    Language/Generator: C/C++
    Specification: wgl
    APIs: wgl=1.0
    Profile: -
    Extensions:
        WGL_ARB_create_context,
        WGL_ARB_create_context_profile,
        WGL_ARB_extensions_string,
        WGL_EXT_extensions_string
    Loader: True
    Local files: False
    Omit khrplatform: False

    Commandline:
        --api="wgl=1.0" --generator="c" --spec="wgl" --extensions="WGL_ARB_create_context,WGL_ARB_create_context_profile,WGL_ARB_extensions_string,WGL_EXT_extensions_string"
    Online:
        http://glad.dav1d.de/#language=c&specification=wgl&loader=on&api=wgl%3D1.0&extensions=WGL_ARB_create_context&extensions=WGL_ARB_create_context_profile&extensions=WGL_ARB_extensions_string&extensions=WGL_EXT_extensions_string
*/


#ifndef WINAPI
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN 1
# endif
# include <windows.h>
#endif

#include "glad.h"

#ifndef __glad_wglext_h_

#ifdef __wglext_h_
#error WGL header already included, remove this include, glad already provides it
#endif

#define __glad_wglext_h_
#define __wglext_h_

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (* GLADloadproc)(const char *name);

#ifndef GLAPI
# if defined(GLAD_GLAPI_EXPORT)
#  if defined(WIN32) || defined(__CYGWIN__)
#   if defined(GLAD_GLAPI_EXPORT_BUILD)
#    if defined(__GNUC__)
#     define GLAPI __attribute__ ((dllexport)) extern
#    else
#     define GLAPI __declspec(dllexport) extern
#    endif
#   else
#    if defined(__GNUC__)
#     define GLAPI __attribute__ ((dllimport)) extern
#    else
#     define GLAPI __declspec(dllimport) extern
#    endif
#   endif
#  elif defined(__GNUC__) && defined(GLAD_GLAPI_EXPORT_BUILD)
#   define GLAPI __attribute__ ((visibility ("default"))) extern
#  else
#   define GLAPI extern
#  endif
# else
#  define GLAPI extern
# endif
#endif

GLAPI int gladLoadWGL(HDC hdc);

GLAPI int gladLoadWGLLoader(GLADloadproc, HDC hdc);

struct _GPU_DEVICE {
    DWORD  cb;
    CHAR   DeviceName[32];
    CHAR   DeviceString[128];
    DWORD  Flags;
    RECT   rcVirtualScreen;
};
DECLARE_HANDLE(HPBUFFERARB);
DECLARE_HANDLE(HPBUFFEREXT);
DECLARE_HANDLE(HVIDEOOUTPUTDEVICENV);
DECLARE_HANDLE(HPVIDEODEV);
DECLARE_HANDLE(HPGPUNV);
DECLARE_HANDLE(HGPUNV);
DECLARE_HANDLE(HVIDEOINPUTDEVICENV);
typedef struct _GPU_DEVICE GPU_DEVICE;
typedef struct _GPU_DEVICE *PGPU_DEVICE;
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define ERROR_INVALID_VERSION_ARB 0x2095
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define ERROR_INVALID_PROFILE_ARB 0x2096
#ifndef WGL_ARB_create_context
#define WGL_ARB_create_context 1
GLAPI int GLAD_WGL_ARB_create_context;
typedef HGLRC (APIENTRYP PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);
GLAPI PFNWGLCREATECONTEXTATTRIBSARBPROC glad_wglCreateContextAttribsARB;
#define wglCreateContextAttribsARB glad_wglCreateContextAttribsARB
#endif
#ifndef WGL_ARB_create_context_profile
#define WGL_ARB_create_context_profile 1
GLAPI int GLAD_WGL_ARB_create_context_profile;
#endif
#ifndef WGL_ARB_extensions_string
#define WGL_ARB_extensions_string 1
GLAPI int GLAD_WGL_ARB_extensions_string;
typedef const char * (APIENTRYP PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);
GLAPI PFNWGLGETEXTENSIONSSTRINGARBPROC glad_wglGetExtensionsStringARB;
#define wglGetExtensionsStringARB glad_wglGetExtensionsStringARB
#endif
#ifndef WGL_EXT_extensions_string
#define WGL_EXT_extensions_string 1
GLAPI int GLAD_WGL_EXT_extensions_string;
typedef const char * (APIENTRYP PFNWGLGETEXTENSIONSSTRINGEXTPROC)();
GLAPI PFNWGLGETEXTENSIONSSTRINGEXTPROC glad_wglGetExtensionsStringEXT;
#define wglGetExtensionsStringEXT glad_wglGetExtensionsStringEXT
#endif

#ifdef __cplusplus
}
#endif

#endif
