SUMMARY = "Custom Raspberry Pi image for eventloop development"
DESCRIPTION = "A minimal image with custom applications and dev tools"

# Start from core-image-minimal as our base
require recipes-core/images/core-image-minimal.bb

# Our custom applications
# This is the name of the package before *_0.1.bb
IMAGE_INSTALL:append = " \
    nano \
    eventloop-revision \
"

# Development tools
EXTRA_IMAGE_FEATURES += " \
    debug-tweaks \
    ssh-server-openssh \
"

# Pi specific
ENABLE_UART = "1"

# Image name
export IMAGE_BASENAME = "rpi-eventloop-image"
