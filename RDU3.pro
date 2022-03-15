QT       += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    paths.cpp \
    simple.pb.c \
    clickablelabel.cpp \
    csrmap.cpp \
#    ecpprog/ecpprog/ecpprog.c \
#    ecpprog/ecpprog/jtag_tap.c \
#    ecpprog/ecpprog/mpsse.c \
    formtest.cpp \
    ltxddecoder.cpp \
    mainwindow.cpp \
    nanopb/pb_common.c \
    nanopb/pb_decode.c \
    nanopb/pb_encode.c \
#    qMDNS/src/qMDNS.cpp \
    qmdnsengine/src/src/abstractserver.cpp \
    qmdnsengine/src/src/bitmap.cpp \
    qmdnsengine/src/src/browser.cpp \
    qmdnsengine/src/src/cache.cpp \
    qmdnsengine/src/src/dns.cpp \
    qmdnsengine/src/src/hostname.cpp \
    qmdnsengine/src/src/mdns.cpp \
    qmdnsengine/src/src/message.cpp \
    qmdnsengine/src/src/prober.cpp \
    qmdnsengine/src/src/provider.cpp \
    qmdnsengine/src/src/query.cpp \
    qmdnsengine/src/src/record.cpp \
    qmdnsengine/src/src/resolver.cpp \
    qmdnsengine/src/src/server.cpp \
    qmdnsengine/src/src/service.cpp \
    rducontroller.cpp \
    rduwindow.cpp \
    rduworker.cpp

HEADERS += \
    paths.h \
    simple.pb.h \
    RDUConstants.h \
    clickablelabel.h \
    csrmap.h \
#    ecpprog/ecpprog/jtag.h \
#    ecpprog/ecpprog/lattice_cmds.h \
#    ecpprog/ecpprog/mpsse.h \
    formtest.h \
    ltxddecoder.h \
    mainwindow.h \
    nanopb/pb.h \
    nanopb/pb_common.h \
    nanopb/pb_decode.h \
    nanopb/pb_encode.h \
#    qMDNS/src/qMDNS.h \
    qmdnsengine/src/include/qmdnsengine/abstractserver.h \
    qmdnsengine/src/include/qmdnsengine/bitmap.h \
    qmdnsengine/src/include/qmdnsengine/browser.h \
    qmdnsengine/src/include/qmdnsengine/cache.h \
    qmdnsengine/src/include/qmdnsengine/dns.h \
    qmdnsengine/src/include/qmdnsengine/hostname.h \
    qmdnsengine/src/include/qmdnsengine/mdns.h \
    qmdnsengine/src/include/qmdnsengine/message.h \
    qmdnsengine/src/include/qmdnsengine/prober.h \
    qmdnsengine/src/include/qmdnsengine/provider.h \
    qmdnsengine/src/include/qmdnsengine/query.h \
    qmdnsengine/src/include/qmdnsengine/record.h \
    qmdnsengine/src/include/qmdnsengine/resolver.h \
    qmdnsengine/src/include/qmdnsengine/server.h \
    qmdnsengine/src/include/qmdnsengine/service.h \
    qmdnsengine/src/src/bitmap_p.h \
    qmdnsengine/src/src/browser_p.h \
    qmdnsengine/src/src/cache_p.h \
    qmdnsengine/src/src/hostname_p.h \
    qmdnsengine/src/src/message_p.h \
    qmdnsengine/src/src/prober_p.h \
    qmdnsengine/src/src/provider_p.h \
    qmdnsengine/src/src/query_p.h \
    qmdnsengine/src/src/record_p.h \
    qmdnsengine/src/src/resolver_p.h \
    qmdnsengine/src/src/server_p.h \
    qmdnsengine/src/src/service_p.h \
    rducontroller.h \
    rduwindow.h \
    rduworker.h

FORMS += \
    formtest.ui \
    mainwindow.ui \
    rduwindow.ui

INCLUDEPATH += $$PWD/qMDNS/src/
INCLUDEPATH += $$PWD/nanopb
INCLUDEPATH += $$PWD/qmdnsengine/src/include/

# Create symbols for dump_syms and symupload
CONFIG += force_debug_info
CONFIG += separate_debug_info

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Crashpad rules for Windows
CONFIG(release, debug|release) {
    SOURCES += main_release.cpp
    win32 {
        INCLUDEPATH += C:/crashpad/crashpad/
        INCLUDEPATH += C:/crashpad/crashpad/out/Default/gen/
        INCLUDEPATH += C:/crashpad/crashpad/third_party/mini_chromium/mini_chromium


        # Crashpad libraries
        LIBS += -LC:/crashpad/crashpad/out/Default/obj/third_party/mini_chromium/mini_chromium/base -lbase
        LIBS += -LC:/crashpad/crashpad/out/Default/obj/client -lclient -lcommon
        LIBS += -LC:/crashpad/crashpad/out/Default/obj/util -lutil

        # System libraries
        LIBS += -lAdvapi32

        # Build variables
        CONFIG(debug, debug|release) {
            EXEDIR = $$OUT_PWD\debug
        }
        CONFIG(release, debug|release) {
            EXEDIR = $$OUT_PWD\release
        }

        # Copy crashpad_handler.exe to output directory
        QMAKE_POST_LINK += "copy /y C:/crashpad/crashpad/out/Default/crashpad_handler.exe $$shell_path($$OUT_PWD)\crashpad"
    }
} else {
    SOURCES += main.cpp
}
