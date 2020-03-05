/*
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-04 19:26:55
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"
// using namespace std;

class Log
{
public:
    /*
     * @desp: 使用日志实例
     * @return:无
     */
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }
    /**
     * @desp: 刷新缓冲区数据
     * @args 参数列表
     * @return: 无
     */
    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }
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
     * @desp: 手动强制刷新缓冲区
     */
    void flush(void);

private:
    Log();
    virtual ~Log();
    /**
     * @desp: 将任务队列中的数据取出并写入日志中
     * @return: 无
     */
    void *async_write_log()
    {
        string single_log;
        // 从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(single_log))
        {
            pthread_mutex_lock(m_mutex);
            fputs(single_log.c_str(), m_fp);
            pthread_mutex_unlock(m_mutex);
        }
    }

private:
    pthread_mutex_t *m_mutex;         // 互斥锁
    char dir_name[128];               // 路径名
    char log_name[128];               // log文件名
    int m_split_lines;                // 日志最大行数
    int m_log_buf_size;               // 日志缓冲区大小
    long long m_count;                // 日志行数记录
    int m_today;                      // 因为按天分类，记录当前时间是那一天
    FILE *m_fp;                       // 打开log的文件指针
    char *m_buf;                      // 缓冲区
    block_queue<string> *m_log_queue; // 阻塞队列
    bool m_is_async;                  // 是否同步标志位
};

//__VA_ARGS__ 是一个可变参数的宏，实现思想就是宏定义中参数列表的最后一个参数为省略号（也就是三个点）

#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, __VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, __VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, __VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, __VA_ARGS__)

#endif
