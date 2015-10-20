#-------------------------------------------------
#
# Project created by QtCreator 2015-10-20T18:23:23
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SenBoy
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.cpp \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.cpp \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.cpp \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.cpp \
    Cartridge.cpp \
    Common.cpp \
    GPU.cpp \
    LR35902.cpp \
    MMU.cpp

HEADERS  += mainwindow.h \
    ../ext/Gb_Snd_Emu-0.1.4/boost/config.hpp \
    ../ext/Gb_Snd_Emu-0.1.4/boost/cstdint.hpp \
    ../ext/Gb_Snd_Emu-0.1.4/boost/static_assert.hpp \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/blargg_common.h \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/blargg_source.h \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.h \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Synth.h \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.h \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.h \
    ../ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.h \
    Cartridge.hpp \
    CommandLine.hpp \
    Common.hpp \
    GBAudioStream.hpp \
    GPU.hpp \
    LR35902.hpp \
    MMU.hpp

INCLUDEPATH += ../ext/Gb_Snd_Emu-0.1.4

FORMS    += mainwindow.ui

LIBS += -L"D:/Git/SFML_Qt/lib"
INCLUDEPATH += D:/Git/SFML_Qt/include

CONFIG(release, debug|release): LIBS += -lsfml-audio-2 -lsfml-graphics-2 -lsfml-window-2 -lsfml-system-2
CONFIG(debug, debug|release): LIBS += -lsfml-audio-2 -lsfml-graphics-2 -lsfml-window-2 -lsfml-system-2

QMAKE_CXXFLAGS += -std=c++0x -Wall