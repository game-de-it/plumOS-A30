plumOS A30 SSH kit
==================

1. Put your public SSH key in:
   plumos/ssh/etc/authorized_keys

2. Copy this whole directory's contents to the SD card root.

3. On the Miyoo A30, run Ports -> Start SSH.

4. Connect from your workstation:
   ssh -p 2222 root@A30_IP_ADDRESS

Logs are written to:
   plumos/ssh/log/dropbear.log
   plumos/ssh/log/network.txt

