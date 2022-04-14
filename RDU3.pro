QT       += core gui network qml quick
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++2a

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
OTHER_FILES += \
    Shortcuts.qml
SOURCES += \
    firmware.cpp \
    lrxddecoder.cpp \
    paths.cpp \
    peekpoke.cpp \
    preferences.cpp \
    radiostate.cpp \
    renderlabel.cpp \
    rotaryaccumulator.cpp \
    serialdecoder.cpp \
    simple.pb.c \
    ecpprog/ecpprog/ecpprog.c \
    ecpprog/ecpprog/jtag_tap.c \
    ecpprog/ecpprog/mpsse.c \
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
    rduworker.cpp

HEADERS += \
    firmware.h \
    lrxddecoder.h \
    paths.h \
    peekpoke.h \
    preferences.h \
    radiostate.h \
    renderlabel.h \
    rotaryaccumulator.h \
    serialdecoder.h \
    simple.pb.h \
    RDUConstants.h \
#    ecpprog/ecpprog/jtag.h \
#    ecpprog/ecpprog/lattice_cmds.h \
#    ecpprog/ecpprog/mpsse.h \
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
    rduworker.h

FORMS += \
    firmware.ui \
    mainwindow.ui \
    peekpoke.ui \
    preferences.ui

INCLUDEPATH += $$PWD/qMDNS/src/
INCLUDEPATH += $$PWD/nanopb
INCLUDEPATH += $$PWD/ecpprog/ecpprog/
INCLUDEPATH += $$PWD/qmdnsengine/src/include/
INCLUDEPATH += $$PWD/tesseract/include
INCLUDEPATH += $$PWD/tesseract/build/include

# Create symbols for dump_syms and symupload
#CONFIG += force_debug_info
#CONFIG += separate_debug_info

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


# Build variables
CONFIG(debug, debug|release) {
    EXEDIR = $$OUT_PWD\debug
}
CONFIG(release, debug|release) {
    EXEDIR = $$OUT_PWD\release
}


LIBS += -L$$PWD/crashpad/crashpad/out/Default/obj/client -lclient -lcommon
LIBS += -L$$PWD/crashpad/crashpad/out/Default/obj/util -lutil
LIBS += -L$$PWD/crashpad/crashpad/out/Default/obj/third_party/mini_chromium/mini_chromium/base -lbase

win32 {
    VCPKG_TUPLE = x64-windows
    VCPKG_LIBS = -larchive -lftdi1 -lleptonica-1.82.0
    LIBS += -lAdvapi32
    QMAKE_POST_LINK += "copy /y $$shell_path($$PWD)\vcpkg_installed\x64-windows\bin\*.dll $$shell_path($$EXEDIR) "
    QMAKE_POST_LINK += "&& copy /y $$shell_path($$PWD)\crashpad\crashpad\out\Default\crashpad_handler.exe $$shell_path($$EXEDIR) "
}
macx {
    VCPKG_TUPLE = x64-osx
    VCPKG_LIBS = -lftdi1 -lleptonica -lgif -ltiff -lwebp -ljpeg -lpng -lopenjp2 -llzma -lz -lusb-1.0 -lcurl -larchive -lbz2 -lzstd -lxml2 -llz4 -liconv
    LIBS += -L$$PWD/tesseract/build/ -ltesseract
    LIBS += -L$$PWD/crashpad/crashpad/out/Default/obj/client -lmig_output

    # System libraries
    LIBS += -L/usr/lib/ -lbsm
    LIBS += -framework AppKit
    LIBS += -framework Security

    QMAKE_POST_LINK += "cp $$PWD/crashpad/crashpad/out/Default/crashpad_handler $$OUT_PWD/RDU3.app/Contents/MacOS/crashpad_handler"
}

INCLUDEPATH += $$PWD/vcpkg_installed/$$VCPKG_TUPLE/include
INCLUDEPATH += $$PWD/vcpkg_installed/$$VCPKG_TUPLE/include/libftdi1/
LIBS += -L$$PWD/vcpkg_installed/$$VCPKG_TUPLE/lib $$VCPKG_LIBS

INCLUDEPATH += $$PWD/crashpad/crashpad/
INCLUDEPATH += $$PWD/crashpad/crashpad/out/Default/gen/
INCLUDEPATH += $$PWD/crashpad/crashpad/third_party/mini_chromium/mini_chromium


# Crashpad rules for Windows
CONFIG(release, debug|release) {
    CONFIG += force_debug_info
    CONFIG += separate_debug_info
    SOURCES += main_crashpad.cpp
    win32 {

        LIBS += -L$$PWD\tesseract\build\Release -ltesseract51
        QMAKE_POST_LINK += "&& $$shell_path($$PWD)\symbols.bat $$shell_path($$PWD)\breakpad\src\tools\windows\symupload\Release\ $$shell_path($$EXEDIR) rdu3 RDU3 0.0.1"
    }
    macx {
        QMAKE_POST_LINK += "&& bash $$PWD/symbols.sh $$PWD $$OUT_PWD rdu3 RDU3 0.0.1"
    }
} else {
    SOURCES += main.cpp
    win32 {
        LIBS += -L$$PWD\tesseract\build\Debug -ltesseract51d
    }
}

DISTFILES += \
    Shortcuts.qml

RESOURCES += \
    rdu.qrc
