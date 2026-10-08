QT       += core gui widgets

CONFIG   += c++17

TARGET    = Overwatch
TEMPLATE  = app

# Disable Windows min/max macros to fix OpenCV header collisions
DEFINES  += NOMINMAX

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    privacythread.cpp

HEADERS += \
    mainwindow.h \
    privacythread.h

FORMS += \
    mainwindow.ui

# ==============================================================================
# OpenCV 5.0 & Windows API Configuration
# ==============================================================================
win32 {
    INCLUDEPATH += "D:/Program/opencv/opencv/build/include"

    CONFIG(debug, debug|release) {
        LIBS += -L"D:/Program/opencv/opencv/build/x64/vc16/lib" -lopencv_world500d
    } else {
        LIBS += -L"D:/Program/opencv/opencv/build/x64/vc16/lib" -lopencv_world500
    }

    LIBS += -lUser32
}
