#-------------------------------------------------
#
# Project created by QtCreator 2013-05-02T10:59:41
#
#-------------------------------------------------

QT       += core#adding the core framework

QT       -= gui#removing the gui portion

TARGET = viewer#targetting the project
#CONFIG   += console#defining that it is console application
#CONFIG   -= app_bundle#

#TEMPLATE = app


SOURCES += main.cpp#adding the main.cpp as source file
LIBS += -L"../dcmtk-3.6.7-install/usr/local/lib" \
-ldcmdata\
-loflog\
-lofstd\
-ldcmimgle\

LIBS += -L"/usr/lib/x86_64-linux-gnu" \
-lz\

LIBS += -L"/usr/local/lib"\
-l:liboficonv.a\



INCLUDEPATH += "../dcmtk-3.6.7-install/usr/local/include/"




INCLUDEPATH += /usr/include/opencv4


LIBS += `pkg-config --cflags --libs opencv4`


INCLUDEPATH += /usr/include/X11
LIBS += -L"/usr/lib"\
-lX11\


INCLUDEPATH += ../gnuplot-cpp

