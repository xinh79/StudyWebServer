/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
using namespace std;

template <class T>
class block_queue
{
public:
    /**
     * @desp: 阻塞任务队列的构造函数
     * @param max_size 队列限制的最大大小
     * @return: 无
     */
    block_queue(int max_size = 1000)
    {
        if (max_size <= 0)
        {
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
        //创建互斥锁和条件变量
        m_mutex = new pthread_mutex_t;
        m_cond = new pthread_cond_t;
        pthread_mutex_init(m_mutex, NULL);
        pthread_cond_init(m_cond, NULL);
    }
    /**
     * @desp: 清除整个循环队列的空间
     * @param 无
     * @return: 无
     */
    void clear()
    {
        pthread_mutex_lock(m_mutex);
        m_size = 0;
        m_front = -1;
        m_back = -1;
        pthread_mutex_unlock(m_mutex);
    }
    /**
     * @desp: 循环队列的析构函数，在这里释放之前申请的资源
     * @param {type} 无
     * @return: 无
     */
    ~block_queue()
    {
        pthread_mutex_lock(m_mutex);
        if (m_array != NULL)
            delete m_array;
        pthread_mutex_unlock(m_mutex);

        pthread_mutex_destroy(m_mutex);
        pthread_cond_destroy(m_cond);

        delete m_mutex;
        delete m_cond;
    }
    /**
     * @desp: 判断循环队列是否满了
     * @param {type} 无
     * @return: bool 已满为true，否则为false
     */
    bool full() const
    {
        pthread_mutex_lock(m_mutex);
        if (m_size >= m_max_size)
        {
            pthread_mutex_unlock(m_mutex);
            return true;
        }
        pthread_mutex_unlock(m_mutex);
        return false;
    }
    /**
     * @desp: 判断循环队列是否为空
     * @param {type} 无
     * @return: bool 为空true，否则为false
     */
    bool empty() const
    {
        pthread_mutex_lock(m_mutex);
        if (0 == m_size)
        {
            pthread_mutex_unlock(m_mutex);
            return true;
        }
        pthread_mutex_unlock(m_mutex);
        return false;
    }
    /**
     * @desp: 返回队首元素
     * @param value 传入传出参数
     * @return: bool 成功为true，否则为false
     */
    bool front(T &value) const
    {
        pthread_mutex_lock(m_mutex);
        if (0 == m_size)
        {
            pthread_mutex_unlock(m_mutex);
            return false;
        }
        value = m_array[m_front];
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    /**
     * @desp: 返回队尾元素
     * @param value 传入传出参数
     * @return: bool 成功为true，否则为false
     */
    bool back(T &value) const
    {
        pthread_mutex_lock(m_mutex);
        if (0 == m_size)
        {
            pthread_mutex_unlock(m_mutex);
            return false;
        }
        value = m_array[m_back];
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    /**
     * @desp: 返回队列任务的个数
     * @param 无
     * @return: int 队列任务的个数
     */
    int size() const
    {
        int tmp = 0;
        pthread_mutex_lock(m_mutex);
        tmp = m_size;
        pthread_mutex_unlock(m_mutex);
        return tmp;
    }
    /**
     * @desp: 返回队列的最大容量值
     * @param {type} 无
     * @return: 队列的最大容量值
     */
    int max_size() const
    {
        int tmp = 0;
        pthread_mutex_lock(m_mutex);
        tmp = m_max_size;
        pthread_mutex_unlock(m_mutex);
        return tmp;
    }
    /**
     * @desp: 往队列添加元素，需要将所有使用队列的线程先唤醒
     * 当有元素push进队列，相当于生产者生产了一个元素
     * 若当前没有线程等待条件变量，则唤醒无意义
     * @param item 传入添加的任务节点
     * @return: 添加成功返回true，失败返回false
     */
    bool push(const T &item)
    {
        pthread_mutex_lock(m_mutex);
        if (m_size >= m_max_size)
        {
            pthread_cond_broadcast(m_cond);
            pthread_mutex_unlock(m_mutex);
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;

        m_size++;
        pthread_cond_broadcast(m_cond);
        pthread_mutex_unlock(m_mutex);

        return true;
    }
    /**
     * @desp: pop时,如果当前队列没有元素,将会等待条件变量
     * @param item 传入传出参数，弹出的任务存在item中
     * @return: 成功返回true，失败返回false
     */
    bool pop(T &item)
    {
        pthread_mutex_lock(m_mutex);
        while (m_size <= 0)
        {
            //
            if (0 != pthread_cond_wait(m_cond, m_mutex))
            {
                pthread_mutex_unlock(m_mutex);
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        pthread_mutex_unlock(m_mutex);
        return true;
    }
    /**
     * @desp: 与上一个pop函数重载，增加了超时处理
     * @param item 传入传出参数；ms_timeout 超时时间
     * @return: 成功返回true，失败返回false
     */
    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        pthread_mutex_lock(m_mutex);
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (0 != pthread_cond_timedwait(m_cond, m_mutex, &t))
            {
                pthread_mutex_unlock(m_mutex);
                return false;
            }
        }

        if (m_size <= 0)
        {
            pthread_mutex_unlock(m_mutex);
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        pthread_mutex_unlock(m_mutex);
        return true;
    }

private:
    pthread_mutex_t *m_mutex; // 互斥锁
    pthread_cond_t *m_cond;   // 条件变量锁
    T *m_array;               // 存放任务的数组
    int m_size;               // 队列中任务的个数
    int m_max_size;           // 队列允许的最大任务个数
    int m_front;              // 队首位置
    int m_back;               // 队尾位置
};

#endif
