# Pertime

Timepie was rename as Pertime on 2020/05.

It doesn't need Timeout service anymore.

## winqtdeploy

From Start, find Qt->Qt 5.15.0 (MSVC 2019 64-bit). This will open cmd.  Go to release directory:

    e:
    cd github\timeout\build-xxx-Release\release
    (or build-xxx-Debug\debug for debug)
    windeployqt.exe timepie.exe

## MacOS
Source code for MacOs can be found in ../macos