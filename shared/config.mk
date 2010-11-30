# Please change to match your preferences


YARI_XTOOLS_TARGET=mips-elf
PREFIX=/home/$(USER)/tools
# enable this if you install into a directory that you don't own
YARI_XTOOLS_SUDO= #sudo
YARI_XTOOLS_BUILDDIR=/tmp/build-tools/build

PATH:=$(YARI_XTOOL_INSTALL_PREFIX)/bin:$(PATH)
MAKE_OPTION=-j4

