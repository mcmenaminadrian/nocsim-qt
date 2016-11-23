#-------------------------------------------------
#
# Project created by QtCreator 2015-12-15T21:43:22
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = noc-qt
TEMPLATE = app
QMAKE_CXXFLAGS += -std=c++11
CONFIG += c++11



SOURCES += main.cpp\
        mainwindow.cpp \
    ControlThread.cpp \
    memory.cpp \
    memorypacket.cpp \
    mux.cpp \
    noc.cpp \
    numberpage.cpp \
    paging.cpp \
    processor.cpp \
    processorFunc.cpp \
    tile.cpp \
    tree.cpp

HEADERS  += mainwindow.h \
    ControlThread.hpp \
    memory.hpp \
    memorypacket.hpp \
    mux.hpp \
    noc.hpp \
    packet.hpp \
    paging.hpp \
    processor.hpp \
    processorFunc.hpp \
    tile.hpp \
    tree.hpp

FORMS    += mainwindow.ui
