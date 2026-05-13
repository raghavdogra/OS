# SBUnix — A 64-bit Operating System Kernel from Scratch

A fully functional x86-64 operating system kernel built from the ground up in C and assembly. Features virtual memory with 4-level paging, preemptive multitasking, ELF binary loading, a Unix-like syscall interface, a tar-based filesystem, and an interactive shell.

Built as an educational OS project — ideal for learning how real operating systems work at the hardware level.

---

## Table of Contents

- [Features](#features)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Running in QEMU](#running-in-qemu)
- [Debugging with GDB](#debugging-with-gdb)
- [Using the Shell](#using-the-shell)
- [Architecture Deep Dive](#architecture-deep-dive)
  - [Boot Process](#boot-process)
  - [Memory Management](#memory-management)
  - [Process Management](#process-management)
  - [System Calls](#system-calls)
  - [Filesystem](#filesystem)
  - [User Space](#user-space)
- [What You Can Learn](#what-you-can-learn)
- [Key Files to Study](#key-files-to-study)

---

## Features

- **x86-64 Long Mode** — runs in 64-bit mode with full hardware setup (GDT, IDT, TSS, PIC)
- **4-Level Paging** — PML4 → PDTP → PDE → PTE page table hierarchy with user/supervisor separation
- **Copy-on-Write Fork** — efficient process creation by sharing pages until written
- **Preemptive Scheduler** — timer-interrupt-driven round-robin scheduler with cooperative yield support
- **ELF64 Loading** — loads and runs standard ELF binaries with proper segment mapping
- **Unix-like Syscalls** — `fork`, `exec`, `wait`, `exit`, `read`, `write`, `open`, `close`, `brk`, `sleep`, `getpid`, `kill`
- **Slab Allocator** — kernel memory allocation via `kmalloc` with slab caching
- **Tar-based Filesystem** — USTAR tar archive embedded in the kernel, parsed into a directory tree at boot
- **Interactive Shell** — `sbush` with builtins (`cd`, `export`, `exit`) and external command execution
- **User Programs** — `ls`, `cat`, `echo`, `ps`, `sleep`, `kill`, and shell script support
- **Minimal libc** — custom C standard library with `printf`, `malloc`, `fork`, `exec`, string functions

---

## Project Structure

```
├── sys/                    # Kernel source
│   ├── main.c              # Kernel entry point and initialization
│   ├── paging.c            # Virtual memory and page table management
│   ├── scheduler.c         # Process scheduler (preemptive + cooperative)
│   ├── syscall.c           # System call handler (int 0x80)
│   ├── elf.c               # ELF64 binary loader
│   ├── initfs.c            # Tar filesystem parser
│   ├── file_handling.c     # File descriptor and I/O management
│   ├── kmalloc.c           # Slab allocator for kernel heap
│   ├── kprintf.c           # Kernel VGA text output
│   ├── idt.c               # Interrupt descriptor table setup
│   ├── gdt.c / gdt.s       # Global descriptor table (64-bit segments, TSS)
│   ├── pic.c               # PIC (8259) interrupt controller
│   ├── timer_isr.c / .s    # Timer interrupt (preemption)
│   ├── kyb_isr.c / .s      # Keyboard interrupt handler
│   ├── syscall.s           # Syscall entry/exit (int 0x80)
│   ├── save_fork_state.s   # Register save for fork
│   ├── switchTask_*.s      # Context switch routines
│   └── linker.script       # Kernel memory layout
├── include/sys/            # Kernel headers
├── libc/                   # User-space C library
│   ├── stdio.c             # printf, puts, gets, getc
│   ├── stdlib.c            # malloc, free, exit, execvpe
│   ├── string.c            # strlen, strcpy, strcmp, memcpy, atoi
│   ├── unistd.c            # fork, exec, read, write, wait, open, close
│   ├── dirent.c            # opendir, readdir, closedir
│   └── signal.c            # Signal handling stubs
├── bin/                    # User programs
│   ├── sbush/sbush.c       # Interactive shell
│   ├── init/init.c         # Init process (PID 1)
│   ├── ls/ls.c             # List directory contents
│   ├── cat/cat.c           # Print file contents
│   ├── echo/echo.c         # Echo arguments
│   ├── ps/ps.c             # List running processes
│   ├── kill/kill.c         # Send signal to process
│   └── sleep/sleep.c       # Sleep for N seconds
├── crt/crt1.c              # C runtime startup (_start)
├── rootfs/                 # Root filesystem (packaged into tar)
│   └── boot/               # FreeBSD bootloader binaries
├── Makefile                # Build system
├── Makefile.config         # Build target selection
└── README.md               # This file
```

---

## Prerequisites

A Linux environment (native Linux or WSL on Windows) with:

```bash
sudo apt-get install gcc make genisoimage syslinux syslinux-utils \
    mtools dosfstools qemu-system-x86
```

| Package | Purpose |
|---------|---------|
| `gcc` | Cross-compiles kernel and userspace (C99, freestanding) |
| `make` | Build system |
| `genisoimage` | Creates ISO image (provides `mkisofs`) |
| `syslinux` | Bootloader for the FAT disk image |
| `mtools` | Manipulates FAT images (`mcopy`) |
| `dosfstools` | Creates FAT filesystem (`mkfs.vfat`) |
| `qemu-system-x86` | x86-64 emulator to run the OS |

---

## Building

```bash
# Full build: kernel + ISO + bootable disk image
make clean && make

# Build just the kernel (skip disk images)
make kernel
```

**Build outputs:**
| File | Description |
|------|-------------|
| `kernel` | ELF64 kernel binary |
| `$USER.iso` | Bootable ISO with kernel and rootfs |
| `$USER.img` | FAT disk image with syslinux bootloader |
| `$USER-data.img` | 16MB raw data disk for AHCI |

---

## Running in QEMU

```bash
qemu-system-x86_64 \
    -drive id=boot,format=raw,file=$USER.img,if=none \
    -drive id=data,format=raw,file=$USER-data.img,if=none \
    -device ahci,id=ahci \
    -device ide-hd,drive=boot,bus=ahci.0 \
    -device ide-hd,drive=data,bus=ahci.1 \
    -gdb tcp::9999 \
    -no-reboot
```

Replace `$USER` with your system username (e.g., `radogra.img`).

**Useful QEMU flags:**

| Flag | Purpose |
|------|---------|
| `-no-reboot` | Halt instead of reboot on crash (easier debugging) |
| `-S` | Pause at startup, wait for GDB to attach |
| `-gdb tcp::9999` | Listen for GDB remote debugging on port 9999 |
| `-m 128` | Set memory size (default 128MB) |

A graphical window will open showing VGA output. The kernel writes to VGA memory (`0xB8000`), so a display is required (SDL window via WSLg or an X server).

---

## Debugging with GDB

In a separate terminal while QEMU is running:

```bash
gdb ./kernel
(gdb) target remote localhost:9999
(gdb) break main
(gdb) continue
```

Useful GDB commands for kernel debugging:
```
info registers          # View CPU registers
x/10i $rip              # Disassemble at current instruction
x/20gx $rsp             # Examine stack
print *CURRENT_TASK     # Inspect current process
```

---

## Using the Shell

Once booted, you'll see the `sbush>` prompt. Available commands:

**Built-in commands:**
| Command | Description |
|---------|-------------|
| `cd <dir>` | Change working directory |
| `export VAR=value` | Set environment variable |
| `exit` | Exit the shell |

**External programs (from `/rootfs/bin/`):**
| Command | Description |
|---------|-------------|
| `ls [path]` | List directory contents |
| `cat <file>` | Display file contents |
| `echo <text>` | Print text to stdout |
| `ps` | List running processes |
| `sleep <seconds>` | Sleep for N seconds |
| `kill <pid>` | Send signal to a process |

**Shell scripts:** Create files starting with `#!/rootfs/bin/sbush` to run as scripts.

---

## Architecture Deep Dive

### Boot Process

```
BIOS → SYSLINUX → MEMDISK → FreeBSD Bootloader → kernel (boot)
                                                       │
                                    ┌──────────────────┘
                                    ▼
                              Clear screen
                              Setup GDT (64-bit segments + TSS)
                              Read BIOS memory map (e820)
                              Initialize 4-level paging
                              Setup IDT (interrupts 0-47 + 0x80)
                              Configure PIC (IRQ remapping)
                              Initialize slab allocator
                              Parse tarfs → directory tree
                              Create idle task + init process
                              Enable interrupts
                              Start scheduler
                                    │
                                    ▼
                              init → fork → sbush
```

### Memory Management

**Virtual address space layout:**
```
0xFFFFFFFF80000000 ─────── Kernel space (direct-mapped to physical)
        ...                 Kernel code, data, page tables
0x00000000C0000000 ─────── User stack (grows down, 256MB)
        ...
        ...                 User heap (grows up via brk/sbrk)
0x0000000000400000 ─────── User code + data (ELF segments)
0x0000000000000000 ─────── NULL (unmapped)
```

**Page table structure (4-level, x86-64):**
```
CR3 → PML4[512] → PDTP[512] → PDE[512] → PTE[512] → 4KB page
```

- Kernel pages: supervisor-only (ring 0)
- User pages: user-accessible (ring 3)
- Copy-on-write: fork shares pages, marks read-only, faults on write

**Kernel heap:** Slab allocator with 8 size classes, each backed by 4KB page slabs.

### Process Management

**Task lifecycle:**
```
fork() → READY → [scheduled] → RUNNING → exit() → ZOMBIE → [reaped by parent]
                     ↑              │
                     │              ▼
                     └── yield() / timer interrupt (preemption)
```

**Context switch** saves/restores all general-purpose registers, stack pointer, instruction pointer, flags, and CR3 (page table root).

**Process tree:**
```
idle (PID 0)
  └── init (PID 1)
        └── sbush (PID 2)
              ├── ls (forked)
              ├── cat (forked)
              └── ...
```

### System Calls

Invoked via `int 0x80` with syscall number in `RAX`:

| # | Syscall | Args | Description |
|---|---------|------|-------------|
| 0 | `read` | fd, buf, count | Read from file or stdin |
| 1 | `write` | fd, buf, count | Write to stdout/stderr |
| 3 | `open` | path, flags | Open file, returns fd |
| 5 | `close` | fd | Close file descriptor |
| 12 | `brk` | addr | Set program break (heap) |
| 35 | `sleep` | seconds | Suspend process |
| 39 | `getpid` | — | Get current process ID |
| 57 | `fork` | — | Clone process (COW) |
| 59 | `execve` | path, argv, envp | Replace process image |
| 60 | `exit` | status | Terminate process |
| 61 | `wait` | status_ptr | Wait for child |

### Filesystem

The filesystem is a USTAR tar archive embedded in the kernel binary at link time:

```
User binaries (bin/*.c)
        │
        ▼
    Compiled to ELF → placed in rootfs/bin/
        │
        ▼
    tar --format=ustar → obj/tarfs
        │
        ▼
    objcopy → obj/tarfs.o (linked into kernel)
        │
        ▼
    At boot: parse_tarfs() → in-memory directory tree (dentry/inode)
```

Files are read directly from the in-memory tar data. There is no write support — the filesystem is read-only.

### User Space

Programs run in **ring 3** with their own virtual address space. Each process gets:
- Separate page tables (forked from parent via COW)
- 10 file descriptors (0=stdin, 1=stdout, 2=stderr, 3-9=files)
- A custom libc linked statically via `rootfs/lib/libc.a`
- CRT startup (`crt1.c`) that calls `main(argc, argv, envp)`

---

## What You Can Learn

This project is a hands-on resource for understanding OS internals. Here's what each part teaches:

### 1. Hardware Initialization & x86-64 Architecture
- **Files:** `sys/gdt.c`, `sys/idt.c`, `sys/pic.c`, `sys/main.c`
- How the CPU transitions from real mode → protected mode → long mode
- Setting up segment descriptors, interrupt tables, and the TSS
- Programming the PIC to remap hardware interrupts

### 2. Virtual Memory & Paging
- **Files:** `sys/paging.c`, `include/sys/paging.h`
- 4-level page table walks and self-referencing entries
- Mapping physical to virtual memory
- Page fault handling and copy-on-write implementation
- Free page management with linked lists

### 3. Interrupt Handling
- **Files:** `sys/idt.c`, `sys/timer_isr.c/.s`, `sys/kyb_isr.c/.s`, `sys/generic_isr.c/.s`
- How hardware interrupts (keyboard, timer) and software interrupts (syscalls) work
- Writing ISRs in assembly with proper register save/restore
- EOI (End of Interrupt) signaling to the PIC

### 4. Process Management & Scheduling
- **Files:** `sys/scheduler.c`, `sys/elf.c`, `include/sys/kernel_threads.h`
- How `fork()` duplicates a process (page tables, VMAs, file descriptors)
- How `exec()` loads an ELF binary into a fresh address space
- Context switching at the register level
- Round-robin scheduling with timer preemption

### 5. System Call Interface
- **Files:** `sys/syscall.c`, `sys/syscall.s`, `libc/unistd.c`
- The `int 0x80` software interrupt mechanism
- Transitioning between user mode (ring 3) and kernel mode (ring 0)
- Implementing POSIX-like syscalls from scratch

### 6. Filesystem Design
- **Files:** `sys/initfs.c`, `sys/file_handling.c`, `include/sys/tarfs.h`
- Parsing a tar archive into an in-memory directory tree
- File descriptors, inodes, and directory entries
- How `open()`, `read()`, and `close()` work internally

### 7. User-Space Programming Without an OS
- **Files:** `libc/*.c`, `crt/crt1.c`, `bin/sbush/sbush.c`
- Building a minimal C library from scratch
- How `_start` → `main()` works via the CRT
- Writing a shell that uses `fork`/`exec`/`wait`

### 8. Linker Scripts & Boot Loading
- **Files:** `sys/linker.script`, `rootfs/boot/loader.rc`
- How the kernel is laid out in memory (virtual vs physical addresses)
- ELF program headers and how bootloaders parse them
- The FreeBSD boot chain (boot0 → boot2 → loader → kernel)

---

## Key Files to Study

If you're new to OS development, read these files in this order:

| Order | File | What You'll Learn |
|-------|------|-------------------|
| 1 | `sys/linker.script` | How kernel memory is laid out |
| 2 | `sys/main.c` | Full kernel boot sequence |
| 3 | `sys/gdt.c` + `sys/gdt.s` | x86-64 segmentation setup |
| 4 | `sys/paging.c` | Virtual memory from scratch |
| 5 | `sys/idt.c` | Interrupt handling setup |
| 6 | `sys/syscall.c` + `sys/syscall.s` | Syscall mechanism |
| 7 | `sys/scheduler.c` | Process scheduling |
| 8 | `sys/elf.c` | Loading user programs |
| 9 | `sys/initfs.c` | Filesystem implementation |
| 10 | `bin/sbush/sbush.c` | User-space shell |
| 11 | `libc/unistd.c` | How libc talks to the kernel |

---

## License

See [LICENSE](LICENSE) for details.
