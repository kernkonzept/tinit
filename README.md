# L4Re tiny init

tinit is an L4Re component that brings up the system on resource constrained
platforms.

# Invocation modes

Tinit can be run as root task next to sigma0 as usual
(`CONFIG_TINIT_RUN_ROOTTASK`). It is also possible to run tinit as sigma0, in
which it will take over the root pager responsibilities too
(`CONFIG_TINIT_RUN_SIGMA0`).

# System initialization

Tinit will read and execute the `rom/inittab` file to start all other tasks in
the system. It is a simple, text based configuration file. Empty lines and
whitespace are ignored. Comments start with `#` and go till the of the line.
Each command must be on a separate line. Numbers can be given as decimal number
or, when starting with a `0x` prefix, as hexadecimal numbers. Arguments to
commands are separated by whitespace.

## Dynamic memory allocation

Internal memory allocation of tinit is done from a statically configured pool.
Its size is defined by the `CONFIG_TINIT_HEAP_SIZE` configuration switch at
compile time.

If `CONFIG_TINIT_DYNAMIC_LOADER` is enabled, the area where a task is started
is allocated dynamically. By default any sufficiently sized RAM region is
used. To restrict the allocation regions a `pool` command can be used:

```
pool ADDR SIZE
```

The command can be used multiple times to define more than one region. It can
take the following optional arguments:

* `nodes:<nodemask>` - Restrict pool to certain nodes. Each bit in the node
  mask corresponds to the respective node. By default a pool is used for all
  nodes in an AMP system.

## Start L4Re tasks

A task is started by the following definition:

```
start hello
end
```

This will simply start `rom/hello`. The `start` command can take the following
optional arguments:

* `node:<num>` - Start task on AMP node `<num>`
* `prio:<priority>` - Set thread priority to `<priority>`
* `reloc:<offset>` - Shift load address by `<offset>` bytes
* `utcb:<order>` - Allocate `L4_PAGE_SIZE << order` bytes kernel-user memory
  for the task.

Command line arguments are passed to the task with the `arg` command that must
come *after* the `start` and *before* the matching `end`. Example:

```
start something
  arg -v
end
```

Inside the `start` block a number of additional commands are available:

* `cap NAME [RIGHTS]`: pass through well-known capability. Currently only
  `sched` is defined, representing the scheduler capability. The optional
  `RIGHTS` argument denotes the mapping rights and is a subset of the letters
  `r` (read), `w` (write), `s` (special), `d` (delete), `n` (don't increase the
  reference counter) and `c` (bind IPC gate).
* `chan NAME [RIGHTS]`: pass an IPC gate `NAME` to the task, optionally
  creating the gate if the name does not exist yet. All tasks that use the
  same channel name get the same IPC gate. See above for the definition of the
  optional `RIGHTS` argument.
* `irq HW_NUM CAP_NAME`: pass through hardware interrupt `HW_NUM` to task
  as named capability `CAP_NAME`.
* `mmio START SIZE`: give VM access to an MMIO region, starting from `START`
  with size `SIZE`.
* `shm START SIZE`: give task access to a shared RAM region, starting from
  `START` with size `SIZE`. Other tasks and VMs can share the same region.

## Define tvmm VMs

There is built-in support for setting up all resources of VMs that can be
passed to `tvmm`, including the right command line arguments. VMs are defined
by the `defvm` command that is terminated by an `end`. The definition must be
enclosed in a `start`/`end` block. The command takes two parameters: the name
of the VM and the scheduling priority of the vCPU (range 1..255):

```
start tvmm
  defvm freertos 0x80
  # ...
  end
end
```

The `defvm` command takes the following optional arguments:

* `asid:<asid>`: set the address space ID (also known as VMID on Arm) to
  `asid`.

Inside a `defvm` block a number of commands are available to configure the
virtual machine:

* `ram START SIZE [off:<offset>]`: give VM exclusive access to RAM, starting
  from `START` with size `SIZE`. Each RAM region can only be allocated once.
  The optional argument `off` can be used to define an offset for the load
  region, e.g. for overlay RAM where the data must be loaded at a different
  address compared to where it is readable by the VM.
* `shm START SIZE`: give VM access to a shared RAM region, starting from
  `START` with size `SIZE`. Other VMs can share the same region.
* `mmio START SIZE`: give VM access to an MMIO region, starting from `START`
  with size `SIZE`. The same region(s) can be assigned to multiple VMs if
  required.
* `irq VIRQ_NUM [HW_NUM]`: pass through hardware interrupt `HW_NUM` as guest
  vIRQ `VIRQ_NUM`. If `HW_NUM` is omitted, the hardware IRQ and vIRQ numbers
  will be the same.
* `irq-priorities MIN MAX`: provide the priority range for the guest vIRQ
  interrupt priorities. The numbers are given as L4Re thread priorities.
* `load IMAGE`: load the ELF-file `IMAGE` into guest memory. This command
  *must* come after the RAM, where the image is loaded, has already been
  allocated to the VM by a `ram` command.
* `reload IMAGE`: passed ELF file to tvmm to load it at vCPU start. This
  is a prerequisite to be able to restart guests on demand.
* `entry ADDR`: set VM entry point to address `ADDR`. Use this command instead
  of `load` if the VM ram content is already populated by another loader or if
  the VM is executed directly from flash.

The following configuration allocates one MB of RAM for the VM. The VM is given
access to some peripheral with it's corresponding interrupt. Finally the
`image.elf` image is loaded:

```
start tvmm
  defvm freertos 0x80
    ram  0x34800000 0x100000
    mmio 0x4011C000  0x4000  # STM0
    irq 56
    irq-priorities 0 0x7f
    load image.elf
  end
end
```

The following full example hosts two VMs in a single `tvmm` instance. The
simple while1-VM does not have access to any peripherals. It merely reserves
the RAM region and loads the VM image:

```
start tvmm utcb:1
  defvm freertos 0x80
    ram  0x34800000 0x100000
    mmio 0x4011C000  0x4000  # STM0
    irq 56
    irq-priorities 0 0x7f
    load image.elf
  end
  defvm while1 0x10
    ram 0x34900000 0x100000
    load while1
    irq-priorities 0 0x0f
  end
end
```

# AMP support

AMP is only supported when `CONFIG_TINIT_RUN_SIGMA0` mode is selected. In this
case it is expected that tinit is run on all AMP cores. At startup tinit will
query the AMP node id and execute only the commands that belong to its node.
All other definitions are ignored.  By default all commands belong to node 0.
To start a task on another node pass the `node:<num>` argument to the `start`
command.

## Example configuration

The easiest way to load tinit multiple times is to rely on
`CONFIG_BID_PIE_VOLUNTARY` or `CONFIG_BID_PIE_ALL` so that tinit can be
relocated automatically. With this configuration bootstrap will be able to
choose a free location by itself.

modules.list:

```
entry quadcore
sigma0[nodes=0:1:2:3] tinit
module hello
module inittab
```

In the inittab the executable must be started for each AMP node individually:

```
start hello node:0 reloc:0x00000
end
start hello node:1 reloc:0x20000
end
start hello node:2 reloc:0x40000
end
start hello node:3 reloc:0x60000
end
```

# Documentation

This package is part of the L4Re Operating System Framework. For documentation
and build instructions see the [L4Re
wiki](https://kernkonzept.com/L4Re/guides/l4re).

# Contributions

We welcome contributions. Please see our contributors guide on
[how to contribute](https://kernkonzept.com/L4Re/contributing/l4re).

# License

Detailed licensing and copyright information can be found in
the [LICENSE](LICENSE.spdx) file.
