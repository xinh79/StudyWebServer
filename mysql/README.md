<!--
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-04 20:27:49
 -->

**目录**

- [数据库连接池](#数据库连接池)
- [模块详解](#模块详解)
- [预备知识](#预备知识)
- [参考文献](#参考文献)

---

## 数据库连接池

数据库连接池

> - 单例模式，保证唯一
> - list 实现连接池
> - 连接池为静态大小
> - 互斥锁实现线程安全

CGI 通用网关接口(Common Gateway Interface)

> - HTTP 请求采用 POST 方式
> - 登录用户名和密码校验
> - 用户注册及多线程注册安全

---

## 模块详解

其中 sign 为单独的模块测试文件，请修改预编译代码，与对应的数据库信息，更改成自己的数据库信息。笔者的测试数据如下：

'-l mysqlclient'编译连接数据库代码，检测数据库是否安装成功：

![1](/mysql/test_mysql1.png)

sign 测试程序对数据进行基本操作演示：

![2](/mysql/test_mysql2.png)

在数据库中查看数据的显示：

![3](/mysql/test_mysql3.png)

MySQL 连接池提供的函数概览：

```cpp
/**
 * @desp: 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
 * @param {type} 无
 * @return: 返回MySQL连接
 */
MYSQL *GetConnection();

/**
 * @desp: 释放连接
 * @conn 待释放的MySQL连接
 * @return: 成功返回true，失败返回false
 */
bool ReleaseConnection(MYSQL *conn);

/**
 * @desp: 销毁所有连接
 * @param {type} 无
 * @return: 无
 */
void DestroyPool();

/**
 * @desp: 获取连接池的实例，单例模式获取一个连接
 * @url 连接地址
 * @User 用户名称
 * @PassWord 用户密码
 * @DataName 数据库名称
 * @Port 端口号
 * @MaxConn 最大连接数
 * @return: 无
 */
static connection_pool *GetInstance(string url, string User, string PassWord,
                                    string DataName, int Port, unsigned int MaxConn);

/**
 * @desp: 返回空闲的连接个数
 * @param 无
 * @return: 空闲连接个数
 */
int GetFreeConn();

/**
 * @desp: 连接池析构函数
 * @param {type} 无
 * @return: 无
 */
~connection_pool();

/**
 * @desp: 连接池构造函数
 * @url 连接地址
 * @User 用户名称
 * @PassWord 用户密码
 * @DataName 数据库名称
 * @Port 端口号
 * @MaxConn 最大连接数
 * @return: 无
 */
connection_pool(string url, string User, string PassWord,
                string DataBaseName, int Port, unsigned int MaxConn);
```

---

## 预备知识

---

## 参考文献
