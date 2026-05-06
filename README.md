# vping

`vping` is a small Linux kernel module that creates a virtual Ethernet
interface for testing ICMP ping behavior.

When the module is loaded, it registers a `vping0` network interface and
creates a procfs configuration file at `/proc/vping/ip`. After you set a target
IPv4 address, the module replies to ICMP echo requests sent to that address.

## Requirements

- Linux
- Kernel headers for your running kernel
- `make`
- GCC or another kernel-compatible C compiler
- Root privileges for loading and unloading the kernel module

Install the required build tools on Debian or Ubuntu:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

Install the required build tools on Fedora:

```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install kernel-devel kernel-headers elfutils-libelf-devel
```

## Build

```bash
make
```

This builds the kernel module as `vping.ko`.

To remove generated build files:

```bash
make clean
```

## Load the Module

```bash
sudo insmod vping.ko
```

Check the kernel log:

```bash
dmesg | tail
```

Confirm that the virtual network interface exists:

```bash
ip link show vping0
```

Bring the interface up:

```bash
sudo ip link set vping0 up
```

## Configure the Ping Address

Set the IPv4 address that `vping` should answer for:

```bash
echo 192.168.10.10 | sudo tee /proc/vping/ip
```

Read the current address:

```bash
cat /proc/vping/ip
```

## Test

The module replies to IPv4 ICMP packets, but it does not implement ARP. For a
local test, add an address to `vping0` and create a static neighbor entry for
the target IP:

```bash
sudo ip addr add 192.168.10.1/24 dev vping0
sudo ip neigh add 192.168.10.10 lladdr 02:00:00:00:00:10 dev vping0 nud permanent
```

Send an ICMP echo request to the configured address:

```bash
ping -c 4 192.168.10.10
```

You can also inspect the module logs while testing:

```bash
dmesg | tail
```

## Unload the Module

Bring the interface down:

```bash
sudo ip link set vping0 down
```

Unload the module:

```bash
sudo rmmod vping
```

Check the kernel log:

```bash
dmesg | tail
```

## Troubleshooting

If `make` fails with a missing kernel build directory, install the kernel
headers that match your running kernel:

```bash
uname -r
```

Then install the matching header package for your distribution.

If `insmod` fails with `Operation not permitted`, make sure Secure Boot is not
blocking unsigned kernel modules, or sign the module according to your
distribution's kernel module signing process.

If `ping` does not receive replies, confirm that:

- The module is loaded.
- `vping0` is up.
- `/proc/vping/ip` contains the address you are pinging.
- You are running the test on a Linux system that allows loading custom kernel
  modules.

## Project Layout

```text
.
|-- Makefile
|-- README.md
`-- src
    `-- vping.c
```

## License

The kernel module declares `MODULE_LICENSE("GPL")`.
