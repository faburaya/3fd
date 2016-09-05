TARGET = IntegrationTests
TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += c++11

CONFIG(release, debug|release): DEFINES += NDEBUG

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
    application.config \
    CMakeLists.txt

INCLUDEPATH += \
    ../btree \
    ../OpenCL

#========================
# 3FD Project Dependency
#========================

win32-g++:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../3FD/ -l3FD
else:win32-g++:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../3FD/ -l3FDd
else:win32:!win32-g++: LIBS += -L$$OUT_PWD/ -l3FD
else:unix:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../3FD/ -l3FD -ldl
else:unix:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../3FD/ -l3FDd -ldl

INCLUDEPATH += $$PWD/../3FD
DEPENDPATH += $$PWD/../3FD

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/lib3FD.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/lib3FD.a
else:win32:!win32-g++: PRE_TARGETDEPS += $$OUT_PWD/3FD.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/lib3FD.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../3FD/lib3FDd.a

#========================
# googletest framework
#========================

win32:win32-g++:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gtest/ -lgtest
else:win32:win32-g++:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gtest/ -lgtest
else:win32:!win32-g++: LIBS += -L$$OUT_PWD/ -lgtestd
else:unix:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gtest/ -lgtest -lpthread
else:unix:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gtest/ -lgtestd -lpthread

INCLUDEPATH += $$PWD/../gtest/include
DEPENDPATH += $$PWD/../gtest/include

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gtest/libgtest.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gtest/libgtestd.a
else:win32:!win32-g++: PRE_TARGETDEPS += $$OUT_PWD/gtest.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gtest/libgtest.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gtest/libgtestd.a

#=======================
# POCO C++ Libraries
#=======================

INCLUDEPATH += /opt/poco-1.7.5/include
DEPENDPATH += /opt/poco-1.7.5/include

# PocoUtil (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoUtil
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoUtild
else:unix:CONFIG(release, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoUtil
else:unix:CONFIG(debug, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoUtild

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoUtil.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoUtild.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/PocoUtil.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/PocoUtild.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoUtil.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoUtild.a

# PocoXML (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoXML
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoXMLd
else:unix:CONFIG(release, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoXML
else:unix:CONFIG(debug, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoXMLd

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoXML.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoXMLd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/PocoXML.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/PocoXMLd.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoXML.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoXMLd.a

# PocoFoundation (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoFoundation
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoFoundationd
else:unix:CONFIG(release, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoFoundation
else:unix:CONFIG(debug, debug|release): LIBS += -L/opt/poco-1.7.5/lib/ -lPocoFoundationd

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoFoundation.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoFoundationd.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/PocoFoundation.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/PocoFoundationd.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoFoundation.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/poco-1.7.5/lib/libPocoFoundationd.a

#=======================
# Boost C++ Libraries
#=======================

INCLUDEPATH += /opt/boost-1.55/include
DEPENDPATH += /opt/boost-1.55/include

# boost/thread (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-d
else:unix:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt
else:unix:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_thread-mt-d

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-d.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_thread-mt.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_thread-mt-d.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_thread-mt-d.a


# boost/system (static library):

win32:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt
else:win32:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-d
else:unix:CONFIG(release, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt
else:unix:CONFIG(debug, debug|release): LIBS += -L/opt/boost-1.55/lib/ -lboost_system-mt-d

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-d.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_system-mt.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/boost_system-mt-d.lib
else:unix:CONFIG(release, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt.a
else:unix:CONFIG(debug, debug|release): PRE_TARGETDEPS += /opt/boost-1.55/lib/libboost_system-mt-d.a
