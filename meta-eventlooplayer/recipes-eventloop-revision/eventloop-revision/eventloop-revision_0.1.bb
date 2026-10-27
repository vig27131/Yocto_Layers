# Recipe metadata
SUMMARY = "Eventloop application for Raspberry Pi"
DESCRIPTION = "An event loop application cross-compiled for Raspberry Pi"
LICENSE = "CLOSED"

LIC_FILES_CHKSUM = "file://LICENSE;md5=<checksum>"

# Inherit cmake — handles everything
inherit cmake pkgconfig

# Where to find our source files
SRC_URI = "file://eventloop_revision.cpp \
           file://CMakeLists.txt"

# Source directory
S = "${WORKDIR}"
