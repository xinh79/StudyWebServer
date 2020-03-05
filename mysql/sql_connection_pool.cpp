#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool *connection_pool::connPool = NULL;

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
connection_pool::connection_pool(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn)
{
	this->url = url;			 // 连接链接
	this->Port = Port;			 // 连接端口
	this->User = User;			 // 连接用户名称
	this->PassWord = PassWord;   // 连接用户的密码
	this->DatabaseName = DBName; // 等待连接的数据库名称

	pthread_mutex_lock(&lock);
	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			cout << "Error:" << mysql_error(con);
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), User.c_str(),
								 PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			cout << "Error: " << mysql_error(con);
			exit(1);
		}
		connList.push_back(con);
		++FreeConn;
	}

	this->MaxConn = MaxConn;
	this->CurConn = 0;
	pthread_mutex_unlock(&lock);
}

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
connection_pool *connection_pool::GetInstance(string url, string User, string PassWord,
											  string DBName, int Port, unsigned int MaxConn)
{
	//先判断是否为空，若为空则创建，否则直接返回现有
	if (connPool == NULL)
	{
		connPool = new connection_pool(url, User, PassWord, DBName, Port, MaxConn);
	}

	return connPool;
}

/**
 * @desp: 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
 * @param {type} 无
 * @return: 返回MySQL连接
 */
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;
	pthread_mutex_lock(&lock);
	//reserve.wait();
	if (connList.size() > 0)
	{
		con = connList.front();
		connList.pop_front();

		--FreeConn;
		++CurConn;

		pthread_mutex_unlock(&lock);
		return con;
	}

	return NULL;
}

/**
 * @desp: 释放当前连接
 * @conn 待释放的MySQL连接
 * @return: 成功返回true，失败返回false
 */
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	pthread_mutex_lock(&lock);
	if (con != NULL)
	{
		connList.push_back(con);
		++FreeConn;
		--CurConn;

		pthread_mutex_unlock(&lock);
		//reserve.post();
		return true;
	}

	return false;
}

/**
 * @desp: 销毁所有连接
 * @param {type} 无
 * @return: 无
 */
void connection_pool::DestroyPool()
{
	pthread_mutex_lock(&lock);
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		CurConn = 0;
		FreeConn = 0;
		connList.clear();
	}
}
/**
 * @desp: 返回空闲的连接个数
 * @param 无
 * @return: 空闲连接个数
 */
int connection_pool::GetFreeConn()
{
	return this->FreeConn;
}
/**
 * @desp: 连接池析构函数
 * @param {type} 无
 * @return: 无
 */
connection_pool::~connection_pool()
{
	DestroyPool();
}
