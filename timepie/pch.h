//
// pch.h
// Header for standard system include files.
//

#pragma once

#define APPLICATION_VERSION "0.1"

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN

#define ORGANIZATION_DOMAIN "pertime.org"
#define ORGANIZATION_NAME "Pertime"
#define APPLICATION_NAME "PertimeDesktop"
const unsigned int SameAppScreenShotInterval = 120000;
