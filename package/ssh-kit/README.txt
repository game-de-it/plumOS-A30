plumOS A30 SSH kit
==================

1. 作業用PCのSSH公開鍵を次のファイルへ入れる:
   plumos/ssh/etc/authorized_keys

2. このdirectoryの中身をすべてSDカード直下へコピーする。

3. Miyoo A30で Ports -> Start SSH を実行する。

4. 作業用PCから接続する:
   ssh -p 2222 root@A30_IP_ADDRESS

logの出力先:
   plumos/ssh/log/dropbear.log
   plumos/ssh/log/network.txt
