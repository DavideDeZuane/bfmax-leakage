# MASTIK

Docker container in Proxmox with Xeon Gold 5215


```
pacman -S --noconfirm base-devel git python msr-tools util-linux gcc
pacman -S --needed base-devel
pacman -S --needed base-devel cmake ninja clang gdb git

```


```
make distclean 2>/dev/null   # pulisci il tentativo precedente
./configure --disable-debug-symbols
make
```




## Setup Container

```
modprobe msr
ls /dev/cpu/0/msr          # ora DEVE esistere sull'host
# rendilo persistente:
echo msr > /etc/modules-load.d/msr.conf
```


```
# aggiungi il passthrough al .conf del CT 199
cat >> /etc/pve/lxc/199.conf <<'EOF'
lxc.cgroup2.devices.allow: c 202:* rwm
lxc.mount.entry: /dev/cpu dev/cpu none bind,optional,create=dir
lxc.cgroup2.devices.allow: c 10:* rwm
lxc.mount.entry: /dev/hugepages dev/hugepages none bind,optional,create=dir
lxc.apparmor.profile: unconfined
EOF

```


Verifica dei requisiti del container privilegiato

```
# MSR accessibili (container privilegiato + bind /dev/cpu)?
ls /dev/cpu/0/msr && modprobe msr 2>/dev/null; rdmsr -p 2 0x1a4   # prefetcher: 'f' se li hai spenti
# topologia: stessa CORE = sibling (per L1), CORE diverso = cross-core (per LLC)
lscpu -e
# hugepages viste dal container
grep Huge /proc/meminfo
```


Disable prefetcher per ridurre il rumore:

```
# prefetcher OFF sul core target (es. CPU 11) — critico per questo test
wrmsr -p 11 0x1a4 0xf && rdmsr -p 11 0x1a4   # deve dire: f
```
