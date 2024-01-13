TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
        usb_communication.cpp

HEADERS += \
    libusb.h \
    usb_communication.hpp
