# SEV-STEP

Academic framework for single-stepping of VM protected by ADM SEV-SNP.


## Installation

Required configuration on the BIOS:

```
Advanced > CPU Common > SNP Memory Coverage (Enable)
Advanced > CPU Common > SEV-ES ASID (100)
Advanced > CPU Common > SEV Support (Enable)
Advanced > CPU Common > SMEE Support (Enable)
Advanced > NB Common > SEV-SNP Support (Enabled)
```

Dependencies:

```
sudo apt install build-essential ninja-build python-is-python3 flex bison \
  libncurses-dev gawk openssl libssl-dev dkms libelf-dev libudev-dev \
  libpci-dev libiberty-dev autoconf llvm libglib2.0-dev libpixman-1-dev \
  pkg-config nasm iasl rsync kmod cpio bc

sudo apt build-dep qemu-system-x86
```

Clone the repo:

```
git clone https://github.com/sev-step/sev-step
cd sev-step
git submodule update --init --recursive
```

Compile and install:

```
./build.sh ovmf
./build.sh qemu
./build.sh kernel
sudo dpkg -i ./kernel-packages/*.deb
```

KVM config:

```
sudo tee /etc/modprobe.d/kvm.conf > /dev/null <<'EOF'
options kvm_amd sev-snp=1 sev=1 sev-es=1
options kvm tdp_mmu=0
EOF
```


At the end do reboot
At this point a new kernel will appear on the `Advanced Setting of debian at startup`


## Virtual Machine

- username: davide
- password: davide


Creating disk for the VM

```
cd ~/sev-step
./local-installation/usr/local/bin/qemu-img create -f qcow2 vm-disk.qcow2 20G
```

Download Ubuntu ISO, beause iis the tested one:

```
wget https://releases.ubuntu.com/24.04/ubuntu-24.04.3-live-server-amd64.iso
```

Launch the VM:

```
cd ~/sev-step
sudo ./launch-qemu.sh -hda vm-disk.qcow2 -cdrom ubuntu-24.04.3-live-server-amd64.iso -vnc :1
```

This will open a terminal and by default will be a running vnc server to connect to the virutal machine.


We can map the remote port of the remote server in the localhost with the following comand

```
ssh -L 5901:localhost:5901 davide@IP_DEL_SERVER
```

And then using tigervnc to open the virtual machine desktop environment.

```
vncviewer localhost:5901
```

But in my case i have a vpn so we can do easly 

```
vcnviewer 192.168.86.90:5901
```



