#ifndef ODPRINT_H
#define ODPRINT_H

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <windows.h>

/* from: http://unixwiz.net/techtips/outputdebugstring.html */
void __cdecl odprintf(const char *format, ...)
{
#ifdef QT_NO_DEBUG
    return;
#endif
    char    buf[4096], *p = buf;
    wchar_t ws[2048];
    va_list args;
    int     n;

    va_start(args, format);
    n = _vsnprintf(p, sizeof buf - 3, format, args); // buf-3 is room for CR/LF/NUL
    va_end(args);

    p += (n < 0) ? sizeof buf - 3 : n;

    while ( p > buf  &&  isspace(p[-1]) )
        *--p = '\0';

    *p++ = '\r';
    *p++ = '\n';
    *p   = '\0';

    swprintf(ws, 2048, L"%hs", buf);
    OutputDebugString(ws);
}
#endif // ODPRINT_H
