#R40 Tina Linux SDK快速指南
# 一、R40 Tina Linux SDK版本下载
## 1.下载repo：
### 1.1.从github上下载repo
```
$ curl https://raw.githubusercontent.com/tinalinux/repo/stable/repo > ~/bin/repo
```
### 1.2.从csdn上下载repo
```
$ curl https://code.csdn.net/tinalinux/repo/blob/stable/repo > ~/bin/repo
```
### 1.3.将repo添加到环境变量
将~/bin目录添加到环境变量：
```
$ chmod +x ~/bin/repo
$ export PATH=~/bin:$PATH
```
## 2.下载R40 Tina Linux SDK V1.0版本
### 2.1从github上下载
```
$ repo init -u https://github.com/tinalinux/manifest -b r40-v1.y -m r40/v1.y.xml
$ repo sync
$ repo start r40-v1.y --all
```
### 2.2.从csdn上下载
```
$ repo init -u https://code.csdn.net/tinalinux/manifest.git -b r40-v1.y -m r40/v1.y-csdn.xml
$ repo sync
$ repo start r40-v1.y --all
```
#二、R40 Tina Linux SDK V1.0版本编译
```
$ source build/envsetup.sh
$ lunch azalea_m2ultra-tina
$ make -j
$ pack [-d]
```
#三、固件烧写
## 3.1.Windows刷机工具phoenixsuit
工具位于 tools/aw_tools,名字为PhoenixSuit_CN.msi。PhoenixSuit是运行在windows上，直接双击即可安装。该工具通过usb来进行刷机的工具。
## 3.2.ubuntu刷机工具LiveSuit
工具位于 tools/aw_tools，名字为LiveSuitV306_For_Linux*.zip，解压压缩包后，通过执行install.sh，该工具默认安装在~/Bin目录下。
安装完后需要通过sudo dpkg -i ~/Bin/aw*.deb来安装驱动，详细可以参考压缩包中的文档。该工具是PhoenixSuit的Linux版本。
注：R40 Tina Linux SDK V1.0版本基于Tina Linux V2.1版本构建
