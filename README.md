# BonDriver_LinuxMirakurun

Linux 版 [EDCB][link_edcb] で使用可能な [Mirakurun][link_mirakurun] 用の BonDriver です。

## ビルド

ビルドには cmake, libssl, libcurl が必要です。

```bash
# Ubuntu
sudo apt update
sudo apt install build-essential cmake git libssl-dev libcurl4-openssl-dev
```

```bash
git clone https://github.com/hendecarows/BonDriver_LinuxMirakurun.git
cd BonDriver_LinuxMirakurun
mkdir build && cd build
cmake ..
make -j
```

## インストール

[EDCB][link_edcb] で使用する場合のインストール方法は以下のとおりです。

### BonDriver の設定

* BonDriver_LinuxMirakurun.ini 内の ServerAddress を修正します
* [EDCB][link_edcb] の BonDriver ディレクトリ `/usr/local/lib/edcb` にコピーします

### EDCB の設定

[EDCB][link_edcb] の EpgDataCap_Bon コマンドでチャンネルスキャンを実行します。

```bash
EpgDataCap_Bon -d BonDriver_LinuxMirakurun.so -chscan
```

EDCB の WEBUI から BonDriver_LinuxMirakurun.so のチューナー数を変更します。

```text
http://192.168.x.x:5510/legacy/setting_bon.html
```

変更を反映させるために EDCB を再起動します。

``` bash
sudo systemctl restart edcb.service
```

## 謝辞

本プロジェクトは、以下のソフトウェアを基に作成されました。

* [BonDriver_LinuxPTX][link_ptx]

## ライセンス

[MIT License][link_mit]

[link_edcb]: https://github.com/xtne6f/EDCB
[link_mirakurun]: https://github.com/chinachu/mirakurun
[link_ptx]: https://github.com/nns779/BonDriver_LinuxPTX
[link_mit]: LICENSE
