plumOS A30 SSH kit
==================

1. 作業用PCのSSH公開鍵を次のファイルへ入れる:
   plumos/ssh/etc/authorized_keys

2. このdirectoryの中身をすべてSDカード直下へコピーする。

3. Miyoo A30で Ports -> Start SSH を実行する。

4. 作業用PCから接続する:
   ssh -p 2222 root@A30_IP_ADDRESS

SSH shell の PATH:
   /mnt/SDCARD/plumos/gnu/bin と /mnt/SDCARD/plumos/bin が自動で先頭に入ります。
   例: crc32, unzip, wget, plumos-fe-control などを full path なしで実行できます。

logの出力先:
   plumos/ssh/log/dropbear.log
   plumos/ssh/log/network.txt
