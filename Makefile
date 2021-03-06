#
# Makefile for ITensor libraries
#
####################################

.NOTPARALLEL:

include options.mk

build: utilities matrix itensor 

utilities: configure
	@echo
	@echo Building Utilities library
	@echo
	cd utilities && make

matrix: configure
	@echo
	@echo Building MatrixRef library
	@echo
	cd matrix && make

itensor: configure
	@echo
	@echo Building ITensor library
	@echo
	cd itensor && make

configure: this_dir.mk

this_dir.mk:
	@echo
	@echo Configure: Writing current dir to this_dir.mk
	@echo THIS_DIR=`pwd` > this_dir.mk

clean:
	cd utilities && make clean
	cd matrix && make clean
	cd itensor && make clean
	cd sample && make clean
	cd sandbox && make clean
	rm -fr include/*
	rm -f lib/*

distclean: clean
	rm -f this_dir.mk options.mk
