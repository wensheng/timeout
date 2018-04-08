set install_dir=..\installer\files
set qtdir=D:\Programs\Qt\Qt5.10.1\5.10.1\msvc2015
if not exist %install_dir% mkdir %install_dir%
if not exist %install_dir%\platforms mkdir %install_dir%\platforms
if not exist %install_dir%\sqldrivers mkdir %install_dir%\sqldrivers
if not exist %install_dir%\styles mkdir %install_dir%\styles

rem dir %qtbin%
rem dir %install_dir%
copy debug\timepie.exe %install_dir%\
copy %qtdir%\bin\Qt5Sqld.dll %install_dir%\
copy %qtdir%\bin\Qt5Networkd.dll %install_dir%\
copy %qtdir%\bin\Qt5Cored.dll %install_dir%\
copy %qtdir%\bin\Qt5Guid.dll %install_dir%\
copy %qtdir%\bin\Qt5Widgetsd.dll %install_dir%\
copy %qtdir%\bin\D3Dcompiler_47d.dll %install_dir%\
copy %qtdir%\bin\libEGLd.dll %install_dir%\
copy %qtdir%\bin\libGLESV2d.dll %install_dir%\
copy %qtdir%\plugins\platforms\qwindowsd.dll %install_dir%\platforms\
copy %qtdir%\plugins\sqldrivers\qsqlited.dll %install_dir%\sqldrivers\
copy %qtdir%\plugins\styles\qwindowsvistastyled.dll %install_dir%\styles\
