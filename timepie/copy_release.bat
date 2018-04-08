set install_dir=..\installer\files
set qtdir=D:\Programs\Qt\Qt5.10.1\5.10.1\msvc2015
if not exist %install_dir% mkdir %install_dir%
if not exist %install_dir%\platforms mkdir %install_dir%\platforms
if not exist %install_dir%\sqldrivers mkdir %install_dir%\sqldrivers
if not exist %install_dir%\styles mkdir %install_dir%\styles

rem dir %qtbin%
rem dir %install_dir%
copy release\timepie.exe %install_dir%\
copy %qtdir%\bin\Qt5Sql.dll %install_dir%\
copy %qtdir%\bin\Qt5Network.dll %install_dir%\
copy %qtdir%\bin\Qt5Core.dll %install_dir%\
copy %qtdir%\bin\Qt5Gui.dll %install_dir%\
copy %qtdir%\bin\Qt5Widgets.dll %install_dir%\
copy %qtdir%\bin\D3Dcompiler_47.dll %install_dir%\
copy %qtdir%\bin\libEGL.dll %install_dir%\
copy %qtdir%\bin\libGLESV2.dll %install_dir%\
copy %qtdir%\plugins\platforms\qwindows.dll %install_dir%\platforms\
copy %qtdir%\plugins\sqldrivers\qsqlite.dll %install_dir%\sqldrivers\
copy %qtdir%\plugins\styles\qwindowsvistastyle.dll %install_dir%\styles\
