PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = tinit

MODE            = sigma0
RELOC_PHYS      = y
DEFAULT_RELOC   = 0x0140000

SRC_CC          = app_task.cc boot_fs.cc cap_alloc.cc debug.cc loader.cc \
                  main.cc registry.cc stubs.cc vm_irq.cc vm_task.cc \
                  $(if $(CONFIG_TINIT_RUN_SIGMA0),page_alloc_sigma0.cc) \
                  $(if $(CONFIG_TINIT_RUN_ROOTTASK),page_alloc_roottask.cc)

SRC_S          := ARCH-$(ARCH)/crt0.S

CAN_PIE_amd64   := y
CAN_PIE_arm     := y
CAN_PIE_arm64   := y
BID_CAN_PIE      = $(CAN_PIE_$(ARCH))

REQUIRES_LIBS  := libc_minimal libc_be_minimal_log_io l4re \
                  $(if $(CONFIG_TINIT_RUN_ROOTTASK),libsigma0)
EXTRA_LIBS     := -ll4sys-direct
DEFINES        += -DL4_CXX_NO_EXCEPTION_BACKTRACE -DL4_MINIMAL_LIBC -DL4_NO_RTTI
LDFLAGS        += --entry=_real_start
CXXFLAGS       += -fno-rtti -fno-exceptions

DEFAULT_HEAP_SIZE = $(CONFIG_TINIT_HEAP_SIZE)
DEFAULT_STACK_SIZE = 0

include $(L4DIR)/mk/prog.mk

DEFINES_arm64-l4f += $(if $(CONFIG_KERNEL_CPU_VIRT),-DCONFIG_KERNEL_CPU_VIRT=1)
