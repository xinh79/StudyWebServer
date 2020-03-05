/*
 * @Descripttion:
 * @version:
 * @Author: Xin Huang
 * @Date: 2020-03-04 13:16:46
 * @LastEditors: Xin Huang
 * @LastEditTime: 2020-03-04 15:42:21
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
// using namespace std;

// 默认构造函数，创建互斥锁，初始化是否同步标志位
Log::Log()
{
    m_count = 0;
    m_mutex = new pthread_mutex_t;
    m_is_async = false;
    pthread_mutex_init(m_mutex, NULL);
}
// log类的析构函数，释放各种内存空间
Log::~Log()
{
    // m_fp 打开log的文件指针
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
    // 销毁互斥锁
    pthread_mutex_destroy(m_mutex);

    if (m_mutex != NULL)
    {
        delete m_mutex;
    }
#if 0
    // 新增代码，如果设置了max_queue_size，则队列会通过new分配
    // 为避免内存泄露，应该手动删除
    if (max_queue_size >= 1) {
        delete m_log_queue;
    }
    // 释放缓冲区m_buf空间
    delete m_buf;
#endif
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
bool Log::init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size)
{
    // 如果设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1)
    {
        m_is_async = true;                                     // 打开同步开关
        m_log_queue = new block_queue<string>(max_queue_size); // 预先分配队列
        pthread_t tid;                                         // 创建线程的ID，并且开始线程
        pthread_create(&tid, NULL, flush_log_thread, NULL);    // flush_log_thread为回调函数，这里表示创建线程异步写日志
    }

    m_log_buf_size = log_buf_size;      // 日志缓冲区的大小
    m_buf = new char[m_log_buf_size];   // 创建日志缓冲区
    memset(m_buf, '\0', sizeof(m_buf)); // 清空日志缓冲区
    m_split_lines = split_lines;        // 设置日志的最大行数

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/'); // 从后往前找到第一个/的位置
    char log_full_name[256] = {0};           // 申请日志的名称存放空间
    // 相当于自定义日志名
    // 若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        // 将/的位置向后移动一个位置，然后复制到logname中
        // p - file_name + 1是文件所在路径文件夹的长度
        // dirname相当于./
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        // 后面的参数跟format有关
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;          // 记录是哪一个天的日志，按天分类
    m_fp = fopen(log_full_name, "a"); // 以在后面追加的形式打开文件
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};       // Linux下的时钟
    gettimeofday(&now, NULL);          // now为传入传出参数
    time_t t = now.tv_sec;             // 设置秒数
    struct tm *sys_tm = localtime(&t); // 获取本地时钟
    struct tm my_tm = *sys_tm;         // 解引用获得本地时间结构体

    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    // 写入一个log，对m_count++, m_split_lines最大行数
    pthread_mutex_lock(m_mutex);
    m_count++;
    // my_tm.tm_mday为每次写日志的时候判断当前时间，m_today是创建文件的时候记录的时间
    // 判断写入的日志行数超出了最大行数，若超过了最大行数，则新建文件
    // 每次写日志时，需要判断当前日志，是不是今天的日志，
    // 如果不是今天的日志，需要将当前打开的关闭，再重新打开
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        char new_log[256] = {0}; // 新的日志文件名称存放空间
        fflush(m_fp);            // 强制刷新缓冲区
        fclose(m_fp);            // fflush()会强迫将缓冲区内的数据写回参数stream 指定的文件中。
        char tail[16] = {0};     // 增加文件尾数组，本质是时间信息
        // 先把时间头写好
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        // 如果是时间不是今天，则创建今天的日志，否则是超过了最大行，
        // 还是之前的日志名，只不过加了后缀，m_count/m_split_lines
        if (m_today != my_tm.tm_mday) // 日志不是同一天的日志
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else // 超出了日志规定的最大行数
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    pthread_mutex_unlock(m_mutex);

    va_list valst;           // 可变参数列表
    va_start(valst, format); // va_start宏初始化变量刚定义的va_list变量，使其指向第一个可变参数的地址。

    string log_str;
    pthread_mutex_lock(m_mutex);

    // 写入的具体时间内容格式
    // snprintf成功返回写字符的总数，其中不包括结尾的null字符
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    // 属于可变参数。用于向字符串中打印数据、数据格式用户自定义，
    // 返回写入到字符数组str中的字符个数(不包含终止符)，最大不超过size
    // 由于我们不用对每个参数做特别的设置，所以直接将其通过vsnprintf写入固定缓冲区
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';     // 加入换行
    m_buf[n + m + 1] = '\0'; // 加入字符串结束标志位
    log_str = m_buf;

    pthread_mutex_unlock(m_mutex);

    // 若m_is_async为true表示不同步，默认为同步
    // 若异步，则将日志信息加入阻塞队列，同步则加锁向文件中写
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str); // 异步，将数据加入到不满的队列中
    }
    else
    {
        pthread_mutex_lock(m_mutex);   // 申请锁，准备写入数据
        fputs(log_str.c_str(), m_fp);  // 写入数据到日志文件中
        pthread_mutex_unlock(m_mutex); // 释放锁
    }

    va_end(valst); // 结束可变参数的获取
}
// 手动强制刷新缓冲区
void Log::flush(void)
{
    pthread_mutex_lock(m_mutex);
    fflush(m_fp); // 强制刷新写入流缓冲区
    pthread_mutex_unlock(m_mutex);
}
