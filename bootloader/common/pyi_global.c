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

/* Can not include via
   #include "pyi_utils.h"
   as this causes a circular include. Declare it manually for now. */
char *pyi_getenv(const char *variable);


#ifdef WIN32
 #include <windows.h>
 #include <direct.h>
 #include <process.h>
 #include <io.h>
 #include <fcntl.h>
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

#ifdef WIN32
static HANDLE _dup_win_handle(HANDLE win_handle)
{
    HANDLE current_process = GetCurrentProcess();
    HANDLE new_handle = 0;
    int okay = DuplicateHandle(
            current_process,        /* source process */
            win_handle,             /* source handle */
            current_process,        /* target process */
            &new_handle,            /* target handle */
            0,                      /* desired access */
            TRUE,                   /* inherit to child processes */
            DUPLICATE_SAME_ACCESS); /* options */
    return okay ? new_handle : 0;
}

static void _set_posix_handle(int posix_handle, HANDLE win_handle)
{
    /* TODO: Error handling. */
    HANDLE dup_win_handle = _dup_win_handle(win_handle);
    int dup_posix_handle = _open_osfhandle((intptr_t) dup_win_handle, _O_TEXT);
    _dup2(dup_posix_handle, posix_handle);
    _close(dup_posix_handle);
}
#endif

void setup_pyi_redirect()
{
#ifdef WIN32
    FILE *tfile = NULL;
    HANDLE outfile;
    char *filename = NULL;
    const char *redirect_option = pyi_getenv("PYI_LOADER_REDIRECT");
    if (!redirect_option)
        return;

    /* Compute the filename. Note that we have to include some
       discriminator that decides between parent and child process.
       Otherwise, in a onefile build creation of the file will
       fail. */
    filename = stb_mprintf("%s.%lu", redirect_option,
                (unsigned long) GetCurrentProcessId());

    /* Can not allocate filename. What can we do? */
    if (!filename)
        return;

    /* Great fun. First we have to redirect the Windows Handles. */
    outfile = CreateFile(
            filename,
            GENERIC_WRITE | GENERIC_READ,       /* desired access */
            FILE_SHARE_READ,                    /* share mode */
            NULL,                               /* security attributes */
            CREATE_ALWAYS,                      /* creation disposition */
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                                /* flags and attributes */
            NULL);                              /* template file */

    SetStdHandle(STD_OUTPUT_HANDLE, outfile);
    SetStdHandle(STD_ERROR_HANDLE, outfile);

    /* But to see stdio output we have to map this to the POSIX handles. */
    _set_posix_handle(1, outfile);
    _set_posix_handle(2, outfile);

    /* Finally, we also have to connect the stdio streams. This should not be
       needed as stdout should be connected to fileno 1 etc, but it seems that
       this is not the case on Windows for GUI applications.

       Unbelievable, but the following is sanctioned by MSDN - see this article:
       http://support.microsoft.com/kb/105305
       IMHO it is a hack to assign to *stdout but if that's the only way,
       what can you do?  -- Torsten */

    tfile = _fdopen(1, "w");
    *stdout = *tfile;
    setvbuf(stdout, NULL, _IONBF, 0);

    tfile = _fdopen(2, "w");
    *stderr = *tfile;
    setvbuf(stderr, NULL, _IONBF, 0);

    free(filename);
#endif
}
