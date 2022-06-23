lib.name := ebur128

ebur128 := libebur128/ebur128/ebur128.c
ebur128~.class.sources := ebur128~.c $(ebur128)

PDLIBBUILDER_DIR=pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

