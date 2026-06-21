plumOS A30 SSH kit
==================

1. このdirectoryの中身をすべてSDカード直下へコピーする。

2. Miyoo A30で Ports -> Start SSH を実行する。

3. 作業用PCから接続する:
   ssh -p 2222 root@A30_IP_ADDRESS

初期SSHパスワード:
   plumos

任意で作業用PCのSSH公開鍵を次のファイルへ入れると、鍵認証も使えます:
   plumos/ssh/etc/authorized_keys

SSH shell の PATH:
   /mnt/SDCARD/plumos/gnu/bin と /mnt/SDCARD/plumos/bin が自動で先頭に入ります。
   例: crc32, unzip, wget, plumos-fe-control などを full path なしで実行できます。
   対話ログインでは stockOS の /etc/profile 後に plumos/ssh/etc/ashrc が PATH を戻します。

logの出力先:
   plumos/ssh/log/dropbear.log
   plumos/ssh/log/network.txt
