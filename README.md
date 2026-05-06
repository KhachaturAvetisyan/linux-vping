## Install requiremetns
```bash
sudo apt install build-essential linux-headers-$(uname -r)
```
OR
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install kernel-devel kernel-headers elfutils-libelf-devel
```

## Build, load, unload commands
```bash
make
sudo insmod vping.ko
dmesg | tail
sudo rmmod vping
dmesg | tail

```

## Show all network interfaces
```bash
ip link show

```

```bash
ip link show vping0
sudo ip link set vping0 up

echo 192.168.10.10 > /proc/vping/ip
cat /proc/vping/ip
```