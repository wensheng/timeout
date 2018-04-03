/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "timeoutservice.h"


int main(int argc, char **argv)
{
    TimeoutService service(argc, argv);
    return service.exec();
}
