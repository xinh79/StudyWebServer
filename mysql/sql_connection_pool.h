/*
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-04 20:19:23
 */
#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../locker/locker.h"

using namespace std;

class connection_pool
{
public:
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

private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	pthread_mutex_t lock;   //互斥锁
	list<MYSQL *> connList; //连接池
	connection_pool *conn;
	MYSQL *Con;

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
	//静态实例
	static connection_pool *connPool;
private:
	string url;			 //主机地址
	string Port;		 //数据库端口号
	string User;		 //登陆数据库用户名
	string PassWord;	 //登陆数据库密码
	string DatabaseName; //使用数据库名
};

#endif
