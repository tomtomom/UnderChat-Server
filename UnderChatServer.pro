#-------------------------------------------------
#
# Project created by QtCreator 2012-02-25T17:33:57
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = UnderChatServer
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += \
    Main.cpp \
    ServerSocket.cpp

HEADERS += \
    SocketException.h \
    ServerSocket.h
