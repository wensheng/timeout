#ifndef ODPRINT_H
#define ODPRINT_H

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef Q_OS_WIN
#include <Windows.h>
#else
#include <syslog.h>
#endif

void odprintf(const char *format, ...)
{
#ifdef QT_NO_DEBUG
    return;
#endif
    va_list args;

#ifdef Q_OS_WIN
    /* from: http://unixwiz.net/techtips/outputdebugstring.html */
    char    buf[4096], *p = buf;
    wchar_t ws[2048];
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
#else
    syslog(LOG_INFO, format, args);
#endif
}
#endif // ODPRINT_H
