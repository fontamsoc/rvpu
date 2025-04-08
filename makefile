# SPDX-License-Identifier: GPL-2.0-only
# Contributed by William Fonkou Tambe

NPROC ?= $(shell nproc)

SUDO := $(shell test $$(id -u) -eq 0 || echo -n sudo)

PROJ := rvpu

.PHONY: all
all: ${PROJ}-toolchain.tar.xz

stamps/prereq: ../${PROJ}
	${SUDO} apt -y install build-essential bsdmainutils dosfstools parted wget rsync cpio unzip dejagnu gettext autoconf automake autotools-dev curl libncurses-dev python3 python3-pip python-is-python3 libelf-dev libmpc-dev libmpfr-dev libgmp-dev gawk bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build cmake libglib2.0-dev libslirp-dev
	${SUDO} mkdir -p /opt/${PROJ}-toolchain
	${SUDO} chown "${USER}" /opt/${PROJ}-toolchain
	mkdir -p $(dir $@) && touch $@

stamps/riscv-gnu-toolchain-build: ../${PROJ}/riscv-gnu-toolchain
	if [ ! -e $(notdir $@) ]; then mkdir -p $(notdir $@) && cd $(notdir $@) && \
		${CURDIR}/$</configure --prefix=/opt/${PROJ}-toolchain --with-arch=rv32ima_zifencei_zicsr --with-abi=ilp32 --with-cmodel=medlow --enable-debug-info; fi
	if [ -e $(notdir $@) ]; then cd $(notdir $@) && make -j${NPROC} newlib; fi
	if [ -n "${USEGIT}" ]; then cd /opt/${PROJ}-toolchain/; if [ ! -d .git ]; then git init; fi; git add .; git commit -m "$@"; fi
	mkdir -p $(dir $@) && touch $@

${PROJ}-toolchain.tar.xz: \
	stamps/prereq \
	stamps/riscv-gnu-toolchain-build
	if [ -n "${USETAR}" ]; then tar -caf $@ --owner=0 --group=0 -C /opt/ --exclude ${PROJ}-toolchain/.git ${PROJ}-toolchain && ls -lh $@ >&2; fi

.PHONY: clean
clean:
	if [ -d ../${PROJ} -a -d stamps ]; then rm -rf *; fi
