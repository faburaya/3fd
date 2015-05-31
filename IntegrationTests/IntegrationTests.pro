TARGET = IntegrationTests
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++11

win32:CONFIG(release, debug|release): DEFINES += NDEBUG
else:unix:!macx:CONFIG(release, debug|release): DEFINES += NDEBUG

DEFINES += \
    ENABLE_3FD_CST \
    ENABLE_3FD_ERR_IMPL_DETAILS \
    TESTING

SOURCES += \
    IntegrationTests.cpp \
    tests_core.cpp \
    tests_gc.cpp \
    tests_opencl.cpp \
    tests_sqlite.cpp

HEADERS += \
    stdafx.h

OTHER_FILES += \
    application.config

INCLUDEPATH += \
    ../btree \
    ../OpenCL

#========================
# 3FD Project Dependency
#========================

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../3FD/release/ -l3FD
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../3FD/debug/ -l3FD
else:unix:!macx: LIBS += -L$$OUT_PWD/../3FD/ -l3FD -ldl

INCLUDEPATH += $$PWD/../3FD
DEPENDPATH += $$PWD/../3FD

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/release/lib3FD.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/debug/lib3FD.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/release/3FD.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/debug/3FD.lib
else:unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../3FD/lib3FD.a

#========================
# googletest framework
#========================

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../x64/ -lgtest
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../x64/ -lgtestd
else:unix:!macx: LIBS += -L$$PWD/../x64/ -lgtest -lpthread

INCLUDEPATH += $$PWD/../gtest/include
DEPENDPATH += $$PWD/../gtest/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../x64/libgtest.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../x64/libgtestd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../x64/gtest.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../x64/gtestd.lib
else:unix:!macx: PRE_TARGETDEPS += $$PWD/../x64/libgtest.a

#=======================
# POCO C++ Libraries
#=======================

INCLUDEPATH += /opt/Poco-1.4.7/include
DEPENDPATH += /opt/Poco-1.4.7/include

# PocoUtil (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoUtil
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoUtild
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoUtil
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoUtild

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoUtil.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoUtild.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/PocoUtil.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/PocoUtild.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoUtil.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoUtild.a

# PocoXML (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoXML
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoXMLd
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoXML
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoXMLd

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoXML.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoXMLd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/PocoXML.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/PocoXMLd.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoXML.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoXMLd.a

# PocoFoundation (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoFoundation
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoFoundationd
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoFoundation
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/Poco-1.4.7/lib/ -lPocoFoundationd

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoFoundation.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoFoundationd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/PocoFoundation.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/PocoFoundationd.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoFoundation.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/Poco-1.4.7/lib/libPocoFoundationd.a

#=======================
# Boost C++ Libraries
#=======================

INCLUDEPATH += /opt/boost-1.55/include
DEPENDPATH += /opt/boost-1.55/include

# boost/thread (static library & runtime):

win32:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-d
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-d

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-d.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_thread-mt.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_thread-mt-d.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-d.a

win32:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-s
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-sd
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-s
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-sd

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-s.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-sd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_thread-mt-s.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_thread-mt-sd.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-s.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-sd.a

# boost/system (static library & runtime):

win32:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-d
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-d

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-d.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_system-mt.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_system-mt-d.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-d.a

win32:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-s
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-sd
else:unix:!macx:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-s
else:unix:!macx:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-sd

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-s.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-sd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_system-mt-s.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_system-mt-sd.lib
else:unix:!macx:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-s.a
else:unix:!macx:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-sd.a
