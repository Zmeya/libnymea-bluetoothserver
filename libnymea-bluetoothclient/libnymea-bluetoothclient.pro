TARGET = nymea-bluetoothclient

QT -= gui
QT += bluetooth network

QMAKE_CXXFLAGS *= -Werror -std=c++11 -g
QMAKE_LFLAGS *= -std=c++11

CONFIG += link_pkgconfig
PKGCONFIG += libsodium

TEMPLATE = lib

DEFINES += VERSION_STRING=\\\"$${VERSION_STRING}\\\"

SOURCES += \
	bluetoothclient.cpp \
	encryptionhandler.cpp \
	loggingcategories.cpp

HEADERS += \
	bluetoothclient.h \
	encryptionhandler.h \
	loggingcategories.h

isEmpty(PREFIX) {
	PREFIX = $$[QT_INSTALL_PREFIX]
}

target.path = $$PREFIX/lib
INSTALLS += target

# install header file with relative subdirectory
for(header, HEADERS) {
	path = $$PREFIX/include/nymea-bluetoothclient/$${dirname(header)}
	eval(headers_$${path}.files += $${header})
	eval(headers_$${path}.path = $${path})
	eval(INSTALLS *= headers_$${path})
}

# Create pkgconfig file
CONFIG += create_pc create_prl no_install_prl
QMAKE_PKGCONFIG_NAME = nymea-bluetoothclient
QMAKE_PKGCONFIG_DESCRIPTION = nymea bluetooth client development library
QMAKE_PKGCONFIG_PREFIX = $$PREFIX
QMAKE_PKGCONFIG_INCDIR = $$PREFIX/include/nymea-bluetoothclient/
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_VERSION = $$VERSION_STRING
QMAKE_PKGCONFIG_FILE = nymea-bluetoothclient
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
