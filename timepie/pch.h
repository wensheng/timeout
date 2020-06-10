//
// pch.h
// Header for standard system include files.
//
#ifndef PCH_H
#define PCH_H

#define APPLICATION_VERSION "0.1"
#define ORGANIZATION_DOMAIN "pertime.org"
#define ORGANIZATION_NAME "Pertime"
#define APPLICATION_NAME "PertimeDesktop"
#define APP_WEBSITE "https://pertime.org/"
#define APP_TEST_WEBSITE "http://192.168.2.116:3399/"
#define DATABASE_NAME "pertime"
#define SETTINGS_GROUP_NAME "Dialog"
#define UPDATE_DEFS_URL "https://pertime.org/PertimeDesktop/updates.json"
//#define WIN32_LEAN_AND_MEAN

const int SameAppScreenShotInterval = 12; // minutes
const int DefaultDataInterval = 300000; // 5 min
const int mainTimerInterval = 30000; // 30 seconds
const int idleThreshold = 600000; // 10 min
const float sameWinDiffThreshold = 0.001;
const int jpeg_quality = 25;
#endif
