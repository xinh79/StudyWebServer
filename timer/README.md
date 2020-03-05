<!--
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-04 20:11:48
 -->

**目录**

- [定时器](#定时器)
- [模块详解](#模块详解)
- [预备知识](#预备知识)
- [参考文献](#参考文献)

---

## 定时器

由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现一个服务器定时器(使用升序链表)，处理这种非活跃连接，释放连接资源。利用 alarm 函数周期性地触发 SIGALRM 信号，该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.

> - 统一事件源
> - 基于升序链表的定时器
> - 处理非活动连接

---

## 模块详解

提供的函数有：

```cpp
/**
 * 往双向链表中添加定时器
 * @timer 双向链表节点
 */
void add_timer(util_timer *timer);
/**
 * 调整指针的定时器节点，更新时间后，需要重新插入到定时器链表中
 * @timer 待更新位置的定时器节点
 */
void adjust_timer(util_timer *timer)；
/**
 * 删除指定的定时器节点
 * @timer 指定的过期定时器
 */
void del_timer(util_timer *timer);
/**
 * SIGALRM信号每次被触发就在其信号处理函数(如果使用同一事件源，则是主函数)中执行一次tick函数，
 * 以处理链表上到期的任务下文会给出使用SIGALRM测试的代码，当然也可以使用其他方式
 * 顺序检查链表中的定时器是否过期，如果过期则删除节点
 */
void tick();
/**
 * add_timer重载函数
 * 找到第一个比timer更晚过期的节点位置，在他之前插入
 * @timer 待插入节点
 * @lst_head 等待插入队列的头结点
 */
void add_timer(util_timer *timer, util_timer *lst_head);
```

---

## 预备知识

---

## 参考文献
