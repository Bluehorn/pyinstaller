/*
 * ****************************************************************************
 * Copyright (c) 2013, PyInstaller Development Team.
 * Distributed under the terms of the GNU General Public License with exception
 * for distributing bootloader.
 *
 * The full license is in the file COPYING.txt, distributed with this software.
 * ****************************************************************************
 */


/*
 * Glogal shared fuctions used in many bootloader files.
 */


/* 
 * Enable use of Sean's Tool Box -- public domain -- http://nothings.org/stb.h.
 * File stb.h.
 * All functions starting with 'stb_' prefix are from this toolbox.
 *
 * This define has to be only in one C source file!
 */
//#define STB_DEFINE  1/* */                                                   
//#define STB_NO_REGISTRY 1 /* No need for Windows registry functions in stb.h. */
#include "stb.h"

#include <stdarg.h>  /* va_list, va_start(), va_end() */
#include <stdio.h>


#ifdef WIN32
 #include <windows.h>
 #include <direct.h>
 #include <process.h>
 #include <io.h>
#endif


/* Text length of MessageBox(). */
#define MBTXTLEN 1024

/*
 * On Windows and with windowed mode (no console) show error messages
 * in message boxes. In windowed mode nothing is written to console.
 */

#if defined(WIN32) && defined(WINDOWED)
    void mbfatalerror(const char *fmt, ...)
    {
        char msg[MBTXTLEN];
        va_list args;

        va_start(args, fmt);
        _vsnprintf_s(msg, MBTXTLEN, MBTXTLEN, fmt, args);
        //vsnprintf(msg, MBTXTLEN, fmt, args);
        va_end(args);

        MessageBox(NULL, msg, "Fatal Error!", MB_OK | MB_ICONEXCLAMATION);
    }

    void mbothererror(const char *fmt, ...)
    {
        char msg[MBTXTLEN];
        va_list args;

        va_start(args, fmt);
        _vsnprintf_s(msg, MBTXTLEN, MBTXTLEN, fmt, args);
        //vsnprintf(msg, MBTXTLEN, fmt, args);
        va_end(args);

        MessageBox(NULL, msg, "Error!", MB_OK | MB_ICONWARNING);
    }
#endif /* WIN32 and WINDOWED */


/* Enable or disable debug output. */

#if defined(WIN32)
/* Shows a message box from formatted text. Arguments are like vprintf.
   Should not be called directly but as handler of the VS macro. */
void mbvs(const char *fmt, va_list args)
{
    char msg[MBTXTLEN];

    _vsnprintf_s(msg, MBTXTLEN, MBTXTLEN, fmt, args);
    //vsnprintf(msg, MBTXTLEN, fmt, args);
    /* Ensure message is timmed to fit the buffer. */
    //msg[MBTXTLEN-1] = '\0';

    MessageBox(NULL, msg, "Tracing", MB_OK);
}
#endif


/* Figure out the default configuration where debug messages go. */
#ifdef LAUNCH_DEBUG
    #if defined(WIN32) && defined(WINDOWED)
        #define PYI_DEBUG_DEFAULT mbvs
    #else
        #define PYI_DEBUG_DEFAULT stbvprint
    #endif
#else
    #define PYI_DEBUG_DEFAULT NULL
#endif

/* The output function for VS debug messages. Set to NULL to disable debug
   output. */
static void (*_pyi_debug_vs)(const char *fmt, va_list args) = PYI_DEBUG_DEFAULT;


/* Implementation for the VS macro. Forwards debugging output to console/message
   box/file/whatever.  Call it like VS("Hello %s", "Dave"); */
void pyi_debug_vs(const char *fmt, ...)
{
    void (*handler)(const char *fmt, va_list args) = _pyi_debug_vs;
    if (handler) {
        va_list args;
        va_start(args, fmt);
        (*handler)(fmt, args);
        va_end(args);
    }
}

/* Called early to allow the user to define the debug variant. */
void setup_pyi_debug()
{
    const char *debug_option = pyi_getenv("PYI_LOADER_DEBUG");
    if (debug_option) {
#ifdef WIN32
        if (strcmp(debug_option, "mbox") == 0) {
            _pyi_debug_vs = mbvs;
        }
#endif
        else if (strcmp(debug_option, "print") == 0) {
            _pyi_debug_vs = stbvprint;
        }
        else if (strcmp(debug_option, "disable") == 0) {
            _pyi_debug_vs = NULL;
        }
    }
}
