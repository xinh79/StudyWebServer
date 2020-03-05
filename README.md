## StudyWebServer

从前到后的一整套操作流程，用于熟悉Web服务器编程

---

Linux 下的简易 web 服务器，实现 web 端用户注册，登录功能，经压力测试可以实现上万的并发连接数据交换。

> - [前言](#前言)
> - [测试结果](#测试结果)
> - [Web端界面](#Web端界面)
> - [Web端测试](Web端测试)
> - [参考文献](#参考文献)

---

## 前言

项目是参考了网上许多的博客与 Github 上面的项目(在文末列出[参考文献](#参考文献))，在他们的基础上做一个整理(其实很多 Github 项目并不能很好的运行，即便按照作者所提供的方法进行修改各种源文件或者配置文件，当然不排除笔者由于技术不足导致的失败)。

笔者对这个项目进行了详细的注释，小到类中成员变量，大到函数结构体均由详细注释，让您在阅读的过程中不那么痛苦。在每一个模块对应的文件夹中有 READNME 文件，您可以通过对其阅读，了解模块的作用，同时笔者还记录一些需要准备的预备知识，与开发时所遇到的问题，当然，仅是部分问题。我还会在之后继续更新，补充，并在上面做修改，比如目前对线程池的实现。在 `thread_pool.cpp` 文件中，是对原有的一个升级。同样给出了详细的注释。

第一次做栽树人，希望后来人能够不要那么痛苦，四处寻找零散的项目。笔者也将再此好好努力，继续学习，拜托学校的稚气，争取春秋招有个好结果！与君共勉。

补充：在 VS Code 中安装 Prettier，让您的代码整洁无比，设置后还能快捷的设置注释。强迫症福音。

---

## 测试结果

测试之前使用 Apache Bench 进行测试，但是提示`socket: Too many open files (24)`，同时查看日志文件可以发现，文件描述符打开至 255，估计也是系统默认的大小。

<div align=center><img src="/img/abtest1.png" height="201"/> </div>

使用`sudo ulimit -n 65535`修改的时候提示：`sudo: ulimit: command not found`。本质上还是因为 ulimit 是一个类似于 shell 的 cd，而不是一个单独的程序。所以无法通过 sudo 提权。

可以使用命令：`sudo sh -c "ulimit -n 65535 && exec su $LOGNAME"`对此设置进行临时更改，也可以修改`sudo vi /etc/security/limits.conf`文件，使其永久生效。压力测试继续：`ab -c 10500 -t 5 "http://192.168.253.129:8888/"`。

PS：如果还是失败，终极办法，切换为 root 用户，然后在 root 权限下对服务器进行测压。

```log
This is ApacheBench, Version 2.3 <$Revision: 1807734 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 192.168.253.129 (be patient)


Server Software:
Server Hostname:        192.168.253.129
Server Port:            8888

Document Path:          /
Document Length:        49 bytes

Concurrency Level(并发数):      10500
Time taken for tests(请求的时间):   5.000 seconds
Complete requests(请求的次数):      14666
Failed requests(失败的请求):        0
Non-2xx responses:      14666
Total transferred(总共传输的字节数http头信息):      1642592 bytes
HTML transferred(实际页面传递的字节数):       718634 bytes
Requests per second(每秒多少个请求):    2933.10 [#/sec] (mean)
Time per request(平均每个用户等待多长时间):       3579.828 [ms] (mean)
Time per request(服务器平均用多长时间处理):       0.341 [ms] (mean, across all concurrent requests)
Transfer rate(每秒获取多少数据):          320.81 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0  308 658.4      0    3057
Processing:     1   62 204.5      2    3336
Waiting:        1   61 203.9      2    3336
Total:          1  370 763.6      2    4774

Percentage of the requests served within a certain time (ms)
  50%      2
  66%      4
  75%     50
  80%   1020(80%的用户的请求1020ms内返回)
  90%   1535
  95%   1776
  98%   3046
  99%   3236
 100%   4774 (longest request)
```

- 并发数：10500
- 请求的时间：5s
- 平均每个用户等待多长时间：3579.828ms
- 服务器平均用多长时间处理：0.341ms

再次做了一此实验，参数一致，但是数据有些波动。(之前是将结果重定向至一个日志文件中，此次方便截屏显示结果)

<div align=center><img src="/img/abtest2.jpg" height="201"/> </div>

---

## Web端界面

界面用于功能的基本测试，所以仅供显示测试效果。

启动服务器后在端口输入`IP:port`：

<div align=center><img src="/img/0.png"/></div>

之后进入选择测试的选项:

<div align=center><img src="/img/1.png"/></div>

选择注册账号，并且输入一个已经存在的用户名进行注册：

<div align=center><img src="/img/2.png"/></div>

输入合法的注册信息，注册成功后会跳转至登录页面，如果输入信息错误后，会跳转至此页面，提示用户名或者密码错误：

<div align=center><img src="/img/4.png"/></div>

输入合法的用户名密码之后，点击确定即可进入欢迎页面，显示测试成功：

<div align=center><img src="/img/5.png"/></div>

## Web端测试

- 测试前确认已安装 MySQL 数据库，其中一些基本操作请见[参考文献](#参考文献)。

```cpp
//建立xinh库
create database xinh;

//创建users表
use xinh;
CREATE TABLE users(
    username char(200) NULL,
    passwd char(200) NULL
);

//添加数据
INSERT INTO users(username, passwd) VALUES('ashior', '123');
```

- 修改 main.c 中的数据库初始化信息

```cpp
//root root为服务器数据库的登录名和密码
connection_pool *connPool=connection_pool::GetInstance("localhost", "root", "1", "xinh", 3306, 5);
```

- 修改 http_conn.cpp 中的数据库初始化信息

```cpp
//root root为服务器数据库的登录名和密码
connection_pool *connPool=connection_pool::GetInstance("localhost", "root", "1", "xinh", 3306, 5);
```

- 修改 http_conn.cpp 中的 root 路径，这就是存放测试页面的目录

```cpp
const char* doc_root="/home/h/StudyWebServer/root";
```

- 启动 server

```shell
./server 8888
```

- 浏览器端按照之前的结果，通过 ifconfig 查看自己的网卡信息，然后在地址栏输入并测试结果。

----

## 参考文献

- [处理http连接](https://www.bbsmax.com/A/RnJW64xBzq/)
- [两猿社项目](https://github.com/twomonkeyclub/TinyWebServer)
- [apache AB 压力测试工具参数说明](https://www.cnblogs.com/ycookie/articles/6668646.html)
- [ab 并发负载压力测试](https://www.cnblogs.com/nulige/p/9370063.html)
- [MySQL 密码设置](https://www.cnblogs.com/super-zhangkun/p/9435974.html)
