cdirs
==================
cdirs 是一个`bash脚本`，用于在多个目录之间快速切换。
>我在使用linux时，总需要在多个目录之间切换。当切换的目录只有两个的时候，可以用＇cd -'。
当多于两个目录的时候呢?只能一次一次的敲路径。不管是绝对路径还是相对路径，即使有tab的补全，
当路径深度一旦超过3层，总会让我很抓狂。忍无可忍，无需再忍，于是我就写了这么一个脚本。

cdirs 初始构思非常简单：

1. **绑定标签**：绑定自定义的标签与路径，并以变量的形式保存在环境变量中
* **切换到标签**：通过切换到标签的形式切换到标签绑定的目录

**但？就这些？too young too simple!!**

=================

##目录
1. [简介](#简介)
2. [特点](#特点)
    1. [共有特点](#共有特点)
    * [cdir特点](#cdir特点)
    * [setdir特点](#setdir特点)
    * [lsdir特点](#lsdir特点)
    * [cldir特点](#cldir特点)
3. [快速安装](#快速安装)
    1. [操作演示](#操作演示)
    * [安装步骤](#安装步骤)
4. [使用指南](#使用指南)
    1. [cdir命令](#cdir命令)
        1. [cdir支持参数](#cdir支持参数)
        * [cdir使用指南](#cdir使用指南)
        * [全局标签](#全局标签)
        * [效果演示](#效果演示)
    * [setdir命令](#setdir命令)
        1. [setdir支持参数](#setdir支持参数)
        * [setdir使用指南](#setdir使用指南)
        * [效果演示](#效果演示)
    * [lsdir命令](#lsdir命令)
        1. [lsdir支持参数](#lsdir支持参数)
        * [lsdir使用指南](#lsdir使用指南)
        * [lsdir嵌入到其他命令](#lsdir嵌入到其他命令)
        * [效果演示](#效果演示)
    * [cldir命令](#cldir命令)
        1. [cldir支持参数](#cldir支持参数)
        * [cldir使用指南](#cldir使用指南)
        * [效果演示](#效果演示)
5. [FAQ](#faq)
6. [寻求帮助与提供建议](#寻求帮助与提供建议)

---

##简介
cdirs 通过便签的形式实现多个目录间的快速切换。有许多人性化的优化，例如"参数，标签，编号，路径"都支持tab补全，例如彩色显示等。
cdirs.sh脚本实现了`cdir`,`setdir`,`lsdir`,`cldir`的4个命令。其中，cdir命令是cd命令的超集，换句话说，你完全可以用cdir命令替换cd命令，即

```Bash
alias cd="cdir"
```

这也是我编写这个脚本的目的，如此确保***不需要太多的学习成本***，也***不用改变什么习惯***，就能使用此脚本。
我断断续续用了1个多月时间编写这脚本，初始版本完成后给同事使用，获得了一致的好评，也希望你能喜欢。

| 命令| 全称 | 功能 | 支持参数 |
|---|---|---|---|
|`cdir` | change directory | 切换目录 | [cdir参数](#cdir支持参数) |
|`setdir` | set directory | 绑定标签与路径 | [setdir参数](#setdir支持参数) |
|`lsdir` | list direcotry | 列出当前绑定的标签 | [lsdir参数](#lsdir支持参数) |
|`cldir` | clear directory | 清除绑定的标签 | [cldir参数](#cldir支持参数) |

##特点
###共有特点

1. 选项,标签,编号,路径等支持tab补全(且对不同选项提供不同的选项参数补全）
* 标签名必须为字母开头，支持数字，字母，和下划线`_`，且对同一个bash，标签名唯一（**重名会覆盖**）
* 支持在`~/.cdir_default`中设定全局标签，bash启动时会自动加载全局标签，因此全局标签可以直接使用(**建议把常用的路径设为全局标签**)
* 标签信息保存为环境变量，因此对不同的bash,标签彼此独立
* 所有路径参数支持绝对路径/相对路径，都支持符号 `.`    `..`    `~` 甚至是 `-`
* 所有命令通过参数`-h|--help`都可以获得使用说明

###cdir特点

1. cd命令的超集，**优先确保cd的功能**，因此能**直接用cdir取代cd**
* 支持通过标签，编号，路径的3种形式切换目录
* 支持`--setdir|--lsdir|--cldir`的选项直接引用为对应命令
* 支持`--reset`选项恢复初次启动bash时的标签状态（会先删除当前所有标签，然后重新加载全局标签）
* 支持`--reload`选项重新加载全局标签
* 支持`-l|--label`,`-p|--path`,`-n|--num`强制指定后续参数类型（对不同的指定类型提供不同tab补全）
* 支持`setdir ,`的特殊命令快速标记当前目录, 以方便`cdir ,`的快速切换返回当前目录
* 特殊标签`,`的编号恒定为0

###setdir特点

1. 支持`-g|--global`参数，设置全局标签（标签信息同时保存在`~/.cdir_defualt`)
* 设定的标签不允许重名，设置的重名标签会覆盖旧便签绑定的路径
* 当标签路径不存在时，创建标签操作会无效（即绑定标签与路径前会先检查路径文件夹是否存在）
* 编号从1开始，为系统自动累加，不支持手动绑定编号与路径，只能通过`cdir --reset`复位编号计数（保证习惯`cdir <num>`的用户能正确切换目录）
* 支持`setdir ,`的特殊命令快速标记当前目录, 以方便`cdir ,`的快速切换返回当前目录
* 特殊标签`,`的编号恒定为0

###lsdir特点

1. 支持显示所有标签或指定显示一个/多个标签
* 支持通过编号，标签，路径的三种形式指定要显示的标签
  1. 通过路径指定标签时，显示的是匹配此路径的所有标签
  * 通过标签名指定标签时，支持标签的**正则表达**匹配多个标签
* 支持`-p|--print`参数，只显示路径以方便嵌入到其他命令中

###cldir特点

1. 支持删除所有便签(需要二次确认)或指定一个/多个标签
* 支持通过编号，标签，路径的三种形式指定要删除的标签
  1. 通过路径指定标签时，删除的是匹配此路径的所有标签
  * 通过标签名指定标签时，支持标签的**正则表达**匹配多个标签
* 支持`-a|--all`参数删除所有标签
* 支持`-g|--global`参数删除当前bash环境标签的同时，删除`~/.cdir_default`内的全局标签
* 支持`-g -a`的组合参数，删除所有bash环境标签和全局标签

##快速安装
###操作演示
###安装步骤

* 获取源码
```Bash
mkdir ~/cdirs
cd ~
git clone https://github.com/gmpy/cdir.git cdirs
```
* 安装
    * 用cdir替换cd
    ```Bash
    chmod u+x ~/cdirs/install.sh
    ~/cdirs/install.sh
    ```

    * 不用cdir替换cd
    ```Bash
    chmod u+x ~/cdirs/install.sh
    ~/cdirs/install.sh --unreplace-cd
    ```
    * 注意
    安装后会在`~/.bashrc`中添加如下内容：
    ```Bash
    # == set for cdir ==
    [ "$(type -t cd)" = "alias" ] && unalias cd
    wait
    source ~/cdirs/cdirs.sh --replace-cd
    # == end for cdir ==
    ```
* 卸载
```Bash
chmod u+x ~/cdirs/install.sh
~/cdirs/install.sh --uninstall
```

##使用指南
###cdir命令
####cdir支持参数

| 选项 | 含义 |
|:---:|:---|
|`-h|--help`| 显示使用帮助 |
|`-l|--label`| 指定后面的参数为标签 |
|`-p|--path`| 指定后面的参数为路径 |
|`-n|--num`| 指定后面的参数为编号 |
|`--setdir|lsdir|cldir`| 使cdir命令等效于`setdir|lsdir|cldir`命令 |
|`--reset`| 复位所有标签（删除当前所有标签，重新计数，加载全局标签） |
|`--reload`| 重新加载所有标签 |

####cdir使用指南
```Bash
$ cdir -h
cdir [--setdir|--lsdir|--cldir] [-h|--help] [-n|--num] [-l|--label] [-p|--path] [--reload] [--reset] <num|label|path>
cdir <,>
--------------
cdir <num|label|path> :
    cd to path that pointed out by num|label|path

cdir <,> :
    cd to special label-path, which is set by "setdir ,", see also "setdir --help"

cdir [--setdir|--lsdir|--cldir] <num|label|path> ... :
    the same as command "setdir|lsdir|cldir",see also "setdir -h", "lsdir -h", "cldir -h"

cdir [-h|--help] :
    show this introduction

cdir [-n|--num] <parameter> :
    describe parameter as num mandatorily

cdir [-l|--label] <parameter> :
    describe parameter as label mandatorily

cdir [-p|--path] <parameter> :
    describe parameter as path mandatorily

cdir [--reload] :
    reload ~/.cdir_default, which record the static label-path

cdir [--reset] :
    clear all label-path and reload ~/.cdir_default, which record the static label-path

Note: cdir is a superset of cd, so you can use it as cd too （In fact, my support for this scripts is to replace cd）
```

####全局标签
#####全局标签简介
正确安装后，bash启动时会自动加载全局标签，因此全局标签可以在启动bash后直接使用
#####全局标签文件路径
`~/.cdir_default`
#####全局标签文件编写规范

```Bash
便签1=路径1
便签2=路径2
...
```

注意：

1. 一行一条全局标签，加载全局标签时依次从上往下加载，因此标签编号从上往下递增
* 不符合规范要求的全局标签无效
* 当标签绑定的路径不存在，此标签无效
* 当文件中全局标签重名时，最后一条重名全局标签才有效

####效果演示

###setdir命令
####setdir支持参数

| 选项 | 含义 |
|:---:|:---|
|`-h|--help`| 显示使用帮助 |
|`-g|--global`| 同时设置为全局标签 |

####setdir使用指南
```Bash
$ setdir -h
setdir [-h|--help] [-g|--global] <label> <path>
setdir <,>
--------------
setdir <label> <path> :
    set label to path, after that, you can use "cdir label" or "cdir num" to go to path (the num is setted by system and you can see by command "lsdir"
    moreover, path strings is support characters like . or .. or ~ or -
    eg. "setdir work ." or "setdir cdirs ~/cdirs" or "setdir last_dir -" or others

setdir <,> :
    set current path as a special label ',' , which is usefull and quick for recording working path , you can go back fastly by "cdir ,"

setdir [-h|--help] :
    show this introduction

setdir [-g|--gloabl] <label> <path> :
    set label to path, moreover, record it in ~/.cdir_default. In this way, you can set this label-path automatically everytimes you run a terminal

Note: label starts with a letter and is a combination of letters, character _ and number
```

####效果演示

###lsdir命令
####lsdir支持参数

| 选项 | 含义 |
|:---:|:---|
|`-h|--help`| 显示使用帮助 |
|`-g|--global`| 同时删除对应的全局标签 |

####lsdir使用指南
```Bash
$ lsdir -h
lsdir [-h|--help] [-p|--print <num|label|path>] <num1|label1|path1|,> <num2|label2|path2|,> ...
--------------
lsdir <num1|label1|path1> <num2|label2|path2> ... :
    list the label-path. if path, list all label-path matching this path; if label, it supports regular expression

lsdir <,> :
    list the special label-path. see also "setdir --help"

lsdir [-h|--help] :
    show this introduction

lsdir [-p|--print <num|label|path>] :
    only print path of label-path, which is usefull to embedded in other commands
    eg. cat `lsdir -p cdirs`/readme.txt => cat /home/user/cdirs/readme.txt
```

####lsdir嵌入到其他命令
例如:
```Bash
$ lsdir cdirs
1 )     cdirs    /home/user/cdirs

$ lsdir -p tina
/home/user/tina

#嵌入到其他命令中举例:
$ ll `lsdir -p cdirs`/gif  #等效于 ll /home/user/cdirs/gif
```

####效果演示

###cldir命令
####cldir支持参数

| 选项 | 含义 |
|:---:|:---|
|`-h|--help`| 显示使用帮助 |
|`-a|--all`| 删除所有便签(需要二次确认) |
|`--reset`| 复位所有标签(删除当前所有标签，重新计数，加载全局标签) |
|`--reload`| 重新加载所有标签 |

####cldir使用指南
```Bash
$ cldir -h
cldir [-h|--help] [-g|--global] [-a|--all] [--reset] [--reload] <num1|label1|path1|,> <num2|label2|path2|,> ...
--------------
cldir <num1|label1|path1> <num2|label2|path2> ... :
    clear the label-path. if path, clear all label-path matching this path; if label, it supports regular expression

cldir <,>:
    clear the special label-path. see also "setdir --help"

cldir [-h|--help] :
    show this introduction

cldir [-g|--gloabl] <num|label|path> :
    unset label to path, moreover, delete it in ~/.cdir_default. see also "setdir -h|--hlep"

cldir [-a|--all] :
    clear all label-path

cldir [--reset] :
    clear all label-path and reload ~/.cdir_default, which record the static label-path

cldir [--reload] :
    reload ~/.cdir_default, which record the static label-path
```

####效果演示

##FAQ
###1. 明明有设定标签，为什么`cd(cdir) + 标签`会进入到其他文件夹？

为了实现用cdir替代cd，会优先确保cd功能，以保证用户的使用习惯不变
因此，当前目录有与标签同名的文件夹时，会优先进入当前目录的同名文件夹而不是标签绑定的路径
例如:
```Bash
$ lsdir cdirs
1 )      cdirs    /home/user/cdirs   #存在名为cdirs的标签

$ ll
...
2757512 drwxrwxr-x  3 user user   4096 10月 21 17:03 cdirs/     #当前路径下存在名为cdirs的文件夹
...

$ cd cdirs  #等效于cd ./cdirs 而不是cd cdirs(标签)
```
可以通过`cd -l cdirs`强制指定cdirs为标签的方式解决

###2. `lsdir -p`嵌入到其他命令中为什么有时会失效？

当绑定的路径存在空格时，`lsdir -p`会失效
例如:
```Bash
$ lsdir -p work
/home/user/my work #此路径存在有空格的文件夹'my work'

$ ll `lsdir -p work`/src    #等效于"ll /home/user/my work/src"，bash解析为"ll /home/user/my" ; "ll work/src"
```
可以通过添加双引号的方式解决：
```Bash
$ ll "`lsdir -p work`/src
```

###3. 通过`cldir + 路径`的方式删除匹配路径的标签时为什么有时会删除错了？
cldir的参数无法判断为路径还是标签｜编号时，会优先理解为标签｜编号
例如:
```Bash
$ lsdir test
10 )      work      /home/user/test     #标签路径最终为test
11 )      test      /home/user/random   #标签为test

$ ll
...
2883727 drwxrwxr-x 2 user user    4096 10月 20 13:51 test/
#当前目录下存在与标签同名的test文件夹, 且此test文件夹恰巧也登记在标签信息中
...

$ cldir test  #只会删除 编号|便签为tina 标签信息
delete: 11 )      test      /home/user/random   #标签为test

$ lsdir
10 )      work    /home/user/test     #test标签的信息被删除了, 而路径为test的便签并没删除
```
可以通过明确标识为路径的方式解决
```Bash
$ cldir ./test
```

###4. 安装了cdir后，`cd [tab][tab]`只列出标签选项，我要列出当前目录的文件夹怎么办？
只需要在`[tab][tab]`前通过各种形式指明后面的参数为路径即可，有如下两种方案：
```Bash
#方案一
$ cdir ./[tab][tab]

#方案二
$ cdir -p [tab][tab]
```

##寻求帮助与提供建议
邮件： gmpy_tiger@163.com

如果你喜欢此应用，请不吝点个赞，谢谢
---
