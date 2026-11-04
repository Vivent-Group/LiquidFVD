#-------------------------------------------------
#
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
#
#-------------------------------------------------


# DEPENDENCIES
# 
# QT (tested with 5.4.0)
# glew (for Windows and Linux builds only)
# glm (tested with 0.9.5.1-1)
# lib3ds

CONFIG	+= qt
QT       += core gui widgets printsupport opengl

#CONFIG += exceptions \
#          rtti

TARGET = LiquidFVD
TEMPLATE = app

# version: <version.txt>.<build>; the build server passes LIQUIDFVD_VERSION (e.g. 0.80.17),
# local builds become <VERSION>.0-dev
LIQUIDFVD_BASE_VERSION = $$cat($$PWD/version.txt)
LIQUIDFVD_VERSION = $$(LIQUIDFVD_VERSION)
isEmpty(LIQUIDFVD_VERSION): LIQUIDFVD_VERSION = $${LIQUIDFVD_BASE_VERSION}.0-dev
DEFINES += LIQUIDFVD_VERSION=\\\"$$LIQUIDFVD_VERSION\\\"
message(LiquidFVD version $$LIQUIDFVD_VERSION)

SOURCES += main.cpp\
    core/undohandler.cpp \
    core/undoaction.cpp \
    core/trackhandler.cpp \
    core/track.cpp \
    core/subfunction.cpp \
    core/smoothhandler.cpp \
    core/sectionhandler.cpp \
    core/section.cpp \
    core/secstraight.cpp \
    core/secgeometric.cpp \
    core/secforced.cpp \
    core/seccurved.cpp \
    core/secbezier.cpp \
    core/saver.cpp \
    core/nolimitsimporter.cpp \
    core/mnode.cpp \
    core/function.cpp \
    core/exportfuncs.cpp \
    osx/common.cpp \
    renderer/trackmesh.cpp \
    renderer/mytexture.cpp \
    renderer/myshader.cpp \
    renderer/myframebuffer.cpp \
    renderer/glviewwidget.cpp \
    ui/transitionwidget.cpp \
    ui/trackwidget.cpp \
    ui/trackproperties.cpp \
    ui/smoothui.cpp \
    ui/qcustomplot.cpp \
    ui/projectwidget.cpp \
    ui/optionsmenu.cpp \
    ui/objectexporter.cpp \
    ui/mytreewidget.cpp \
    ui/myqdoublespinbox.cpp \
    ui/mainwindow.cpp \
    ui/importui.cpp \
    ui/graphwidget.cpp \
    ui/graphhandler.cpp \
    ui/exportui.cpp \
    ui/draglabel.cpp \
    ui/conversionpanel.cpp \
    core/secnlcsv.cpp \
    ui/backupmanager.cpp \
    ui/theme.cpp \
    ui/projectmedia.cpp \
    ui/timelinestrip.cpp \
    ui/updater.cpp \
    ui/curveeditor.cpp \
    core/funcsnapshot.cpp

HEADERS  += core/undohandler.h \
    core/undoaction.h \
    core/trackhandler.h \
    core/track.h \
    core/subfunction.h \
    core/smoothhandler.h \
    core/sectionhandler.h \
    core/section.h \
    core/secstraight.h \
    core/secgeometric.h \
    core/secforced.h \
    core/seccurved.h \
    core/secbezier.h \
    core/saver.h \
    core/nolimitsimporter.h \
    core/mnode.h \
    core/function.h \
    core/exportfuncs.h \
    osx/common.h \
    renderer/trackmesh.h \
    renderer/mytexture.h \
    renderer/myshader.h \
    renderer/myframebuffer.h \
    renderer/glviewwidget.h \
    ui/transitionwidget.h \
    ui/trackwidget.h \
    ui/trackproperties.h \
    ui/smoothui.h \
    ui/qcustomplot.h \
    ui/projectwidget.h \
    ui/optionsmenu.h \
    ui/objectexporter.h \
    ui/mytreewidget.h \
    ui/myqdoublespinbox.h \
    ui/mainwindow.h \
    ui/importui.h \
    ui/graphwidget.h \
    ui/graphhandler.h \
    ui/exportui.h \
    ui/draglabel.h \
    ui/conversionpanel.h \
    lenassert.h \
    core/secnlcsv.h \
    ui/backupmanager.h \
    ui/theme.h \
    ui/projectmedia.h \
    ui/timelinestrip.h \
    ui/updater.h \
    ui/curveeditor.h \
    core/funcsnapshot.h \
    thirdparty/miniaudio/miniaudio.h

FORMS    += ui/transitionwidget.ui \
    ui/trackwidget.ui \
    ui/trackproperties.ui \
    ui/smoothui.ui \
    ui/projectwidget.ui \
    ui/optionsmenu.ui \
    ui/objectexporter.ui \
    ui/mainwindow.ui \
    ui/importui.ui \
    ui/graphwidget.ui \
    ui/exportui.ui \
    ui/conversionpanel.ui

!unix:!macx {
    INCLUDEPATH += "./ui/"
    INCLUDEPATH += "./renderer/"
    INCLUDEPATH += "./core/"

    # library locations - override on the qmake command line, e.g.
    #   qmake GLEW_DIR=C:/libs/glew GLM_DIR=C:/libs/glm LIB3DS_DIR=C:/libs/lib3ds
    # or build without the .3ds exporter:  qmake CONFIG+=no3ds
    isEmpty(GLEW_DIR):   GLEW_DIR   = C:/Development/Libraries/glew-1.12.0
    isEmpty(GLM_DIR):    GLM_DIR    = C:/Development/Libraries/glm
    isEmpty(LIB3DS_DIR): LIB3DS_DIR = C:/Development/Libraries/lib3ds-20080909

    INCLUDEPATH += $$GLEW_DIR/include
    INCLUDEPATH += $$GLM_DIR

    RC_FILE = winicon.rc

    # sources are UTF-8 (symbols like the play button or the degree sign)
    win32-msvc*: QMAKE_CXXFLAGS += /utf-8
    win32-msvc*: DEFINES += _USE_MATH_DEFINES NOMINMAX

    LIBS += -lOpenGL32
    LIBS += -lGlU32
    LIBS += $$GLEW_DIR/lib/Release/x64/glew32.lib

    no3ds {
        DEFINES += FVD_NO_LIB3DS
    } else {
        INCLUDEPATH += $$LIB3DS_DIR/src
        LIBS += -L$$LIB3DS_DIR/lib -llib3ds
    }
}

unix:!macx {
    INCLUDEPATH += "./ui/"
    INCLUDEPATH += "./renderer/"
    INCLUDEPATH += "./core/"

    LIBS += -lGL
    LIBS += -lGLU
    LIBS += -lGLEW

    LIBS += -lX11
    LIBS += -L /usr/local/lib/
    no3ds {
        DEFINES += FVD_NO_LIB3DS
    } else {
        LIBS += -l3ds
    }
}

macx {
    ICON = fvd.icns
    QMAKE_INFO_PLIST = ./osx/resources/Info.plist

    INCLUDEPATH += "./ui/"
    INCLUDEPATH += "./renderer/"
    INCLUDEPATH += "./core/"
    # glm location can be passed to qmake: qmake GLM_DIR=/path/to/glm ; CONFIG+=no3ds builds without lib3ds
    isEmpty(GLM_DIR): GLM_DIR = ./glm
    INCLUDEPATH += $$GLM_DIR
    INCLUDEPATH += "/usr/local/include/"
    DEFINES += GL_SILENCE_DEPRECATION

    LIBS += -framework Foundation -framework Cocoa
    no3ds {
        DEFINES += FVD_NO_LIB3DS
    } else {
        LIBS += -L /usr/local/lib/
        LIBS += -l3ds
    }

    QMAKE_CFLAGS_X86_64 += -mmacosx-version-min=10.6
    QMAKE_CXXFLAGS_X86_64 = $$QMAKE_CFLAGS_X86_64
    QMAKE_CXXFLAGS_RELEASE += -O2

    OBJECTIVE_SOURCES += \
        osx/Document.mm \
        osx/MainDelagate.mm \
        osx/NSApplicationMain.mm

    HEADERS += \
        osx/Document.h \
        osx/MainDelagate.h \
        osx/NSApplicationMain.h

    SOURCES +=  \
        osx/Init.cpp
}

# miniaudio (public domain / MIT-0): decodes WAV/MP3/FLAC and plays them, no extra libraries needed
SOURCES += thirdparty/miniaudio/miniaudio.c
INCLUDEPATH += "./thirdparty/miniaudio/"
unix:!macx: LIBS += -ldl -lpthread -lm
macx: LIBS += -framework CoreFoundation -framework CoreAudio -framework AudioToolbox

RESOURCES += \
    resources.qrc

OTHER_FILES += \
    winicon.rc \
    background.png \
    metal.png \
    shaders/normals.vert \
    shaders/track.vert \
    shaders/track.frag \
    shaders/sky.vert \
    shaders/sky.frag \
    shaders/simpleSM.vert \
    shaders/simpleSM.frag \
    shaders/shadowVolume.vert \
    shaders/shadowVolume.frag \
    shaders/oculus.vert \
    shaders/oculus.frag \
    shaders/occlusion.vert \
    shaders/occlusion.frag \
    shaders/normals.frag \
    shaders/metal.dat \
    shaders/floor.vert \
    shaders/floor.frag \
    shaders/debug.vert \
    shaders/debug.frag \
    shaders/marker.vert \
    shaders/marker.frag \
    metalnormals.png \
    readme.txt \
    sky/negx.jpg \
    sky/negy.jpg \
    sky/negz.jpg \
    sky/posx.jpg \
    sky/posy.jpg \
    sky/posz.jpg
