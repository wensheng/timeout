# timeout

diretories timeout and controller are obsolete.

## test service

To test service installation, we must run cmd as Admin.

## winqtdeploy

because we build msvc2015 32bit, we need to use under "Visual Studio 2015", "VS2015 x86 Native Tools Command Prompt" to open cmd, go to release dir, then run:

    d:\Programs\Qt\Qt5.10.1\5.10.1\msvc2015_32\bin\windeployqt.exe timeout.exe
    d:\Programs\Qt\Qt5.10.1\5.10.1\msvc2015_32\bin\windeployqt.exe timepie.exe

Then combine 2 directories.

