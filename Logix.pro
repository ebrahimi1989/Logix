QT -= gui
QT += network
CONFIG += c++17 console
CONFIG -= app_bundle

# Prevent redefinition of SPDLOG_HEADER_ONLY
DEFINES += SPDLOG_HEADER_ONLY

# Include paths for spdlog and nlohmann/json (changed to relative paths)
INCLUDEPATH += $$PWD/spdlog/include
INCLUDEPATH += $$PWD/nlohmann/include

SOURCES += \
    loggerfacade.cpp \
    main.cpp

HEADERS += \
    loggerfacade.h

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
