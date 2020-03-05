<!--
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-04 20:09:23
 -->

**目录**

- [同步/异步日志系统](#同步/异步日志系统)
- [模块详解](#模块详解)
- [预备知识](#预备知识)
- [参考文献](#参考文献)

---

## 同步/异步日志系统

同步/异步日志系统主要涉及了两个模块，一个是日志模块，一个是阻塞队列模块,其中加入阻塞队列模块主要是解决异步写入日志做准备.

> - 自定义阻塞队列
> - 单例模式创建日志
> - 同步日志
> - 异步日志
> - 实现按天、超行分类

---

## 模块详解

日志文件提供的方法：

```cpp
/**
 * @desp: 使用日志实例
 * @return:无
 */
static Log *get_instance()
/**
 * @desp: 刷新缓冲区数据
 * @args 参数列表
 * @return: 无
 */
static void *flush_log_thread(void *args)
/**
 * 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
 * 异步需要设置阻塞队列的长度，同步不需要设置
 * @file_name 文件名称
 * @log_buf_size 日志缓冲区大小
 * @split_lines 日志最大行数
 * @max_queue_size 队列最大长度
 * @return 是否初始化成功-bool
 */
bool init(const char *file_name, int log_buf_size = 8192,
        int split_lines = 5000000, int max_queue_size = 0);
/**
 * 写入日志，根据等级写入对应的日志信息
 * @level 写入日志的等级
 * @format 写入数据，可变参数va_list结合使用，最后通过vsnprintf写入缓冲区
 */
void write_log(int level, const char *format, ...);
/**
 * @desp: 将任务队列中的数据取出并写入日志中
 * @return: 无
 */
void *async_write_log()
```

循环数组实现的阻塞队列为日志文件提供了底层支持。对其提供的方法如下：

```cpp
/**
 * @desp: 阻塞任务队列的构造函数
 * @param max_size 队列限制的最大大小
 * @return: 无
 */
block_queue(int max_size = 1000);
/**
 * @desp: 清除整个循环队列的空间
 * @param 无
 * @return: 无
 */
void clear();
/**
 * @desp: 循环队列的析构函数，在这里释放之前申请的资源
 * @param {type} 无
 * @return: 无
 */
~block_queue();
/**
 * @desp: 判断循环队列是否满了
 * @param {type} 无
 * @return: bool 已满为true，否则为false
 */
bool full() const;
/**
 * @desp: 判断循环队列是否为空
 * @param {type} 无
 * @return: bool 为空true，否则为false
 */
bool empty() const;
/**
 * @desp: 返回队首元素
 * @param value 传入传出参数
 * @return: bool 成功为true，否则为false
 */
bool front(T &value) const;
/**
 * @desp: 返回队尾元素
 * @param value 传入传出参数
 * @return: bool 成功为true，否则为false
 */
bool back(T &value) const;
/**
 * @desp: 返回队列任务的个数
 * @param 无
 * @return: int 队列任务的个数
 */
int size() const;
/**
 * @desp: 返回队列的最大容量值
 * @param {type} 无
 * @return: 队列的最大容量值
 */
int max_size() const;
/**
 * @desp: 往队列添加元素，需要将所有使用队列的线程先唤醒
 * 当有元素push进队列，相当于生产者生产了一个元素
 * 若当前没有线程等待条件变量，则唤醒无意义
 * @param item 传入添加的任务节点
 * @return: 添加成功返回true，失败返回false
 */
bool push(const T &item);
/**
 * @desp: pop时,如果当前队列没有元素,将会等待条件变量
 * @param item 传入传出参数，弹出的任务存在item中
 * @return: 成功返回true，失败返回false
 */
bool pop(T &item);
/**
 * @desp: 与上一个pop函数重载，增加了超时处理
 * @param item 传入传出参数；ms_timeout 超时时间
 * @return: 成功返回true，失败返回false
 */
bool pop(T &item, int ms_timeout);
```

---

## 预备知识

`gettimeofday()`函数来得到时间。它的精度可以达到微妙。

```cpp
#include<sys/time.h>
int gettimeofday(struct  timeval*tv,struct  timezone *tz )
```

`fflush()`会强迫将缓冲区内的数据写回参数 stream 指定的文件中。在日志系统中，主要是用于刷新缓冲器，将其写入日志中。

VA_LIST 是在 C 语言中解决变参问题的一组宏，变参问题是指参数的个数不定，可以是传入一个参数也可以是多个；可变参数中的每个参数的类型可以不同，也可以相同；可变参数的每个参数并没有实际的名称与之相对应，用起来是很灵活。

```cpp
#include "stdarg.h"
#include <iostream>

int sum(char* msg, ...);

int main()
{
    int total = 0;
    total = sum("hello world", 1, 2, 3);
    std::cout << "total = " << total << std::endl;
    system("pause");
    return 0;
}

int sum(char* msg, ...)
{
    // 定义一个具有va_list型的变量，这个变量是指向参数的指针。
    va_list vaList;
    // 第一个参数指向可变列表的地址,地址自动增加，第二个参数位固定值
    va_start(vaList, msg);
    std::cout << msg << std::endl;
    int sumNum = 0;
    int step;
    // va_arg第一个参数是可变参数的地址，第二个参数是传入参数的类型，
    // 返回值就是va_list中接着的地址值，类型和va_arg的第二个参数一样
    // va_arg 取得下一个指针
    while ( 0 != (step = va_arg(vaList, int)))
    {
        //不等于0表示，va_list中还有参数可取
        sumNum += step;
    }
    //结束可变参数列表
    va_end(vaList);
    return sumNum;
}
```

va_list 的使用方法：

1. 首先在函数中定义一个具有 va_list 型的变量，这个变量是指向参数的指针。
2. 然后用 va_start 宏初始化变量刚定义的 va_list 变量，使其指向第一个可变参数的地址。
3. 然后 va_arg 返回可变参数，va_arg 的第二个参数是你要返回的参数的类型（如果多个可变参数，依次调用 va_arg 获取各个参数）。
4. 最后使用 va_end 宏结束可变参数的获取。

```cpp
int vsprintf(char *str, const char *format, va_list arg)
```

---

## 参考文献

- [gettimeofday()函数的使用方法](https://www.cnblogs.com/long5683/p/9999746.html)
- [va_list 函数学习](https://www.cnblogs.com/qiwu1314/p/9844039.html)
- [网络编程定时器一：使用升序链表](https://www.cnblogs.com/zhangkele/p/10468826.html)
