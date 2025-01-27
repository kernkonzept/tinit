PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

SYSTEMS         = arm-l4f arm64-l4f
TARGET          = tinit

MODE            = sigma0
RELOC_PHYS      = y
DEFAULT_RELOC   = 0x0140000

SRC_CC          = app_task.cc boot_fs.cc cap_alloc.cc debug.cc loader.cc \
                  main.cc registry.cc stubs.cc vm_irq.cc vm_task.cc

SRC_CC-$(CONFIG_TINIT_RUN_SIGMA0)   += page_alloc_sigma0.cc
SRC_CC-$(CONFIG_TINIT_RUN_ROOTTASK) += page_alloc_roottask.cc

SRC_S          := ARCH-$(ARCH)/crt0.S
PRIVATE_INCDIR := $(SRC_DIR)/ARCH-$(ARCH)

CAN_PIE_arm     := y
CAN_PIE_arm64   := y
BID_CAN_PIE      = $(CAN_PIE_$(ARCH))

REQUIRES_LIBS  := libc_minimal libc_be_minimal_log_io libc_minimal_l4re \
                  $(if $(CONFIG_TINIT_RUN_ROOTTASK),libsigma0)
DEFINES        += -DL4_CXX_NO_EXCEPTION_BACKTRACE -DL4_MINIMAL_LIBC -DL4_NO_RTTI
LDFLAGS        += --entry=_real_start
CXXFLAGS       += -fno-rtti -fno-exceptions

DEFAULT_HEAP_SIZE = $(CONFIG_TINIT_HEAP_SIZE)
DEFAULT_STACK_SIZE = 0

include $(L4DIR)/mk/prog.mk
