<!--
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-05 00:04:40
 -->

## http 连接处理类

---

根据状态转移,通过主从状态机封装了 http 连接类。其中,主状态机在内部调用从状态机,从状态机将处理状态和数据传给主状态机

> - 客户端发出 http 连接请求
> - 从状态机读取数据,更新自身状态和接收数据,传给主状态机
> - 主状态机根据从状态机状态,更新自身状态,决定响应请求还是继续读取

## 模块详解

与之前的 FTP 服务器不同，此服务器端对 HTTP 请求的处理做了十分细致的划分，包括对每一个字段的拼接，而不是在一个函数中划分，通过众多小函数的调用，完成整个响应状态，响应头，空行和数据的拼接以及发送。下面提供函数概览：

```cpp
/**
 * @desp: 获取数据库连接池，然后连接数据库获取数据，保存用户信息与密码，初始化users
 * @param {type} 无
 * @return: 无
 */
void http_conn::initmysql_result();
/**
 * @desp: 对文件描述符设置非阻塞
 * @param fd 待设置的文件描述符
 * @return: fcntl的返回值，失败返回-1
 */
int setnonblocking(int fd);
/**
 * @desp: 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
 * EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，
 * 如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
 * @param epollfd epoll文件描述；fd 新增的文件描述符；
 * @return: 无
 */
void addfd(int epollfd, int fd, bool one_shot);
/**
 * @desp: 从内核时间表删除描述符
 * @param epollfd epoll 树的文件描述符；fd 待删除的文件描述符
 * @return: 无
 */
void removefd(int epollfd, int fd);
/**
 * @desp: 将事件重置为EPOLLONESHOT
 * @param epollfd epoll 树的文件描述符；fd 待修改的文件描述符；ev 为对应的事件如EPOLLIN/EPOLLOUT
 * @return: 无
 */
void modfd(int epollfd, int fd, int ev);
/**
 * @desp: 关闭连接，关闭一个连接，客户总量减一
 * @param real_close
 * @return: 无
 */
void http_conn::close_conn(bool real_close);
/**
 * @desp: 初始化连接，私有成员，只能自己调用
 * @param sockfd 文件描述符，addr 结构体信息
 * @return: 无
 */
void http_conn::init(int sockfd, const sockaddr_in &addr);
/**
 * @desp: 初始化新接受的连接，check_state默认为分析请求行状态
 * @param 无
 * @return: 无
 */
void http_conn::init()
/**
 * @desp: 从状态机，用于分析出一行内容
 * @param 无
 * @return: 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
 */
http_conn::LINE_STATUS http_conn::parse_line();
/**
 * @desp: 循环读取客户数据，直到无数据可读或对方关闭连接
 * 非阻塞ET工作模式下，需要一次性将数据读完
 * @param 无
 * @return: 读取成功返回true，失败返回false
 */
bool http_conn::read_once();
/**
 * @desp: 解析http请求行，获得请求方法，目标url及http版本号
 * @param text 客户端发送来的数据
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::parse_request_line(char *text);
/**
 * @desp: 解析http请求头信息，http的一个头部信息
 * @param text 客户端发送的数据信息
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::parse_headers(char *text);
/**
 * @desp: 判断http请求是否被完整读入
 * @param text 客户端发送的数据信息
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::parse_content(char *text);
/**
 * @desp: 处理请求数据
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::process_read();
/**
 * @desp: 对http请求发出响应，包括页面请求，用户登录，用户注册
 * @param {type} 无
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::do_request();
/**
 * @desp: 解除文件内存映射
 * @param {type} 无
 * @return: 无
 */
void http_conn::unmap();
/**
 * @desp: 发送数据
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::write();
/**
 * @desp: 添加响应，可变参数，用于各种报文拼接
 * @param 可变参数
 * @return: 成功为true，否则为false
 */
bool http_conn::add_response(const char *format, ...);
/**
 * @desp: 添加响应状态行
 * @param status 状态 title 头
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_status_line(int status, const char *title);
/**
 * @desp: 添加响应状态头
 * @param content_len 头长度
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_headers(int content_len);
/**
 * @desp: 添加内容实体的长度-Content-Length字段
 * @param content_len 内容长度
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_content_length(int content_len);
/**
 * @desp: 添加内容类型-Content-Type字段
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_content_type();
/**
 * @desp: 添加响应报文的Connection字段，指定是否保持连接
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_linger();
/**
 * @desp: 添加响应报文的空行-添加\r\n字段
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_blank_line();
/**
 * @desp: 添加响应内容
 * @param content 根据状态码确定响应内容
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_content(const char *content);
/**
 * @desp: 处理写请求
 * @param ret http响应报文的状态码
 * @return: 成功返回true，失败返回false
 */
bool http_conn::process_write(HTTP_CODE ret);
/**
 * @desp: 处理http请求的线程调用的入口函数-回调函数
 * @param {type} 无
 * @return: 无
 */
void http_conn::process();
```

---

## 预备知识

EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个 socket 的话，需要再次把这个 socket 加入到 EPOLL 队列里。

C 库函数 `char *strpbrk(const char *str1, const char *str2)` 检索字符串 str1 中第一个匹配字符串 str2 中字符的字符，不包含空结束字符。也就是说，依次检验字符串 str1 中的字符，当被检验字符在字符串 str2 中也包含时，则停止检验，并返回该字符位置。

```cpp
char *strpbrk(const char *str1, const char *str2);

#include <stdio.h>
#include <string.h>

int main ()
{
   const char str1[] = "abcde2fghi3jk4l";
   const char str2[] = "34";
   char *ret;

   ret = strpbrk(str1, str2);
   if(ret)
   {
      printf("第一个匹配的字符是： %c\n", *ret);
   }
   else
   {
      printf("未找到字符");
   }

   return(0);
}
// 第一个匹配的字符是： 3
```

`strcasecmp()` 用来比较参数 s1 和 s2 字符串，比较时会自动忽略大小写的差异。

```cpp
int strcasecmp (const char *s1, const char *s2);
```

C 库函数 `size_t strspn(const char *str1, const char *str2)` 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。

```cpp
size_t strspn(const char *str1, const char *str2);
#include <stdio.h>
#include <string.h>

int main ()
{
   int len;
   const char str1[] = "ABCDEFG019874";
   const char str2[] = "ABCD";

   len = strspn(str1, str2);

   printf("初始段匹配长度 %d\n", len );

   return(0);
}
// 初始段匹配长度 4
```

C 库函数 `char *strrchr(const char *str, int c)` 在参数 str 所指向的字符串中搜索最后一次出现字符 c (一个无符号字符)的位置。

```cpp
#include <stdio.h>
#include <string.h>

int main ()
{
   int len;
   const char str[] = "https://www.github.com";
   const char ch = '.';
   char *ret;

   ret = strrchr(str, ch);

   printf("|%c| 之后的字符串是 - |%s|\n", ch, ret);

   return(0);
}
// |.| 之后的字符串是 - |.com|
```

\_vsnprintf 是 C 语言库函数之一，属于可变参数。用于向字符串中打印数据、数据格式用户自定义。

---

## 参考文献

- [HTTP 返回码详解](https://blog.csdn.net/kongxianglei5313/article/details/80636167)
- [epoll EPOLLONESHOT 事件](https://blog.csdn.net/le119126/article/details/46364399)
- [Linux errno 详解](https://www.cnblogs.com/x_wukong/p/10848069.html)
- [菜鸟教程 - strpbrk](https://www.runoob.com/cprogramming/c-function-strpbrk.html)
- [菜鸟教程 - strspn](https://www.runoob.com/cprogramming/c-function-strspn.html)
- [菜鸟教程 - strrchr](https://www.runoob.com/cprogramming/c-function-strrchr.html)
- [高级 I/O 之 readv 和 writev 函数](https://blog.csdn.net/weixin_36750623/article/details/84579243)
