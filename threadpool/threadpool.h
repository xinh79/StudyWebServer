#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../locker/locker.h"
template <typename T>
class threadpool
{
public:
    /**
     * 线程池的构造函数
     * @thread_number 线程池中线程的数量
     * @max_request 请求队列中最多允许的、等待处理的请求的数量
     */
    threadpool(int thread_number = 8, int max_request = 10000);
    ~threadpool();
    /**
     * 往队列中添加任务
     * @request 请求即为类型T的任务
     */
    bool append(T *request);

private:
    /**
     * 工作线程运行的函数，它不断从工作队列中取出任务并执行之
     * 此函数设置为静态是因为需要指定位线程执行的回调函数
     * 此回调函数本质调用run()函数
     * @arg 执行任务所需要的参数
     */
    static void *worker(void *arg);
    /**
     * 此函数为实际执行回调方法的函数
     */
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
};
template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
    // 输入的参数不合法
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    // 为线程创建存放线程ID空间
    m_threads = new pthread_t[m_thread_number];
    // 创建失败返回
    if (!m_threads)
        throw std::exception();
    // 创建成功后，根据thread_number数量，依次创建线程
    for (int i = 0; i < thread_number; ++i)
    {
        //printf("create the %dth thread\n",i);
        // 创建工作线程，并为其指定回调函数
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        // 避免手动分离，做线程分离，无需手动调用pthread_join函数
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads; // 销毁创建的空间
    m_stop = true;      // 设置标志位表示线程池关闭
}

template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();                    // 加锁，准备往共享的队列中添加任务
    if (m_workqueue.size() > m_max_requests) // 超出队列的最大值，释放锁
    {
        m_queuelocker.unlock(); // 释放锁
        return false;           // 添加失败
    }
    m_workqueue.push_back(request); // 将任务加入到队列末尾
    m_queuelocker.unlock();         // 释放锁
    m_queuestat.post();             // 调用post表示任务队列中有任务
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuestat.wait();      // 等待处理任务
        m_queuelocker.lock();    // 对任务队列进行加锁访问
        if (m_workqueue.empty()) // 如果发现任务队列已经为空
        {
            m_queuelocker.unlock(); // 释放锁
            continue;
        }
        T *request = m_workqueue.front(); // 从队列头获取任务
        m_workqueue.pop_front();          // 将已经被获取的任务弹出
        m_queuelocker.unlock();           // 释放锁
        if (!request)                     // 没有参数不做处理
            continue;
        request->process(); // 执行回调函数
    }
}
#endif
