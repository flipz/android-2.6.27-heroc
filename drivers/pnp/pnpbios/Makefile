#
# Makefile for the kernel PNPBIOS driver.
#

pnpbios-proc-$(CONFIG_PNPBIOS_PROC_FS) = proc.o

obj-y := core.o bioscalls.o rsparser.o $(pnpbios-proc-y)

ifeq ($(CONFIG_PNP_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
