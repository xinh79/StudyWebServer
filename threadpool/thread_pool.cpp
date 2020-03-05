#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

// 10s检测一次
#define DEFAULT_TIME 3
// 如果queue_size > MIN_WAIT_TASK_NUM 添加新的线程到线程池
#define MIN_WAIT_TASK_NUM 10
// 每次创建和销毁线程的个数
#define DEFAULT_THREAD_VARY 10

// 各子线程任务结构体
typedef struct {
	// 函数指针，回调函数
    void *(*function)(void *);
    // 上面函数的参数
    void *arg;
} threadpool_task_t;

// 描述线程池相关信息
template<typename T>
class threadpool_t {
public:
	// 构造函数，需要输入最小的线程数，最大的线程数，队列的最大等待长度
	threadpool_t(int min_thr_num, int max_thr_num, int queue_max_size);
	~threadpool_t();
	// 工作者线程
	static void *worker_thread(void* arg);
	void *threadpool_thread();
	// 管理者线程
	static void *worker_adjust(void* arg);
	void *adjust_thread();

	// 检查某一个线程是否可用
	static bool is_thread_alive(pthread_t tid);
	// 向线程池中添加一个任务
	int threadpool_add(void*(*function)(void *arg), void *arg);
	// 返回总的线程数目
	int threadpool_all_threadnum();
	// 返回工作线程数目
	int threadpool_busy_threadnum();
    // 返回工作队列大小
    int threadpool_queue_size();
	// 打印线程池的信息
	void threadpool_info();

    void shutdown_pool() {
        this->shutdown = true;
    }

private:
	// 用于锁住本结构体
    pthread_mutex_t lock;
	// 记录忙状态线程个数的琐——busy_thr_num
    pthread_mutex_t thread_counter;
	// 当任务队列满时，添加任务的线程阻塞，等待此条件变量
    pthread_cond_t queue_not_full;
	// 任务队列里不为空时，通知等待任务的线程
    pthread_cond_t queue_not_empty;

	// 存放线程池中每个线程的tid。数组
    pthread_t *threads;
	// 存管理线程tid
    pthread_t adjust_tid;
	// 任务队列
	// std::list<T*> workqueue;
    threadpool_task_t *task_queue;

	// 线程池最小线程数
    int min_thr_num;
	// 线程池最大线程数
    int max_thr_num;
	// 当前存活线程个数
    int live_thr_num;
	// 忙状态的线程个数
    int busy_thr_num;
	// 要销毁的线程个数
    int wait_exit_thr_num;

    int queue_front;                    /* task_queue队头下标 */
    int queue_rear;                     /* task_queue队尾下标 */
    int queue_size;                     /* task_queue队中实际任务数 */
    int queue_max_size;                 /* task_queue队列可容纳任务数上限 */

	// 标志位，线程池使用状态，true或false
    bool shutdown;
    // 创建内存池的资源
	int __threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);
    // 释放内存池的资源
    int __threadpool_destroy(threadpool_t *pool);
    int __threadpool_free(threadpool_t *pool);
};

template<typename T>
threadpool_t<T>::
threadpool_t(int min_thr_num, int max_thr_num, int queue_max_size) {
    // printf("threadpool_t ---------- start\n");
    __threadpool_create(min_thr_num, max_thr_num, queue_max_size);
    // printf("threadpool_t ---------- end\n");
}

// 创建线程池相关数据
template<typename T>
int threadpool_t<T>::
__threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size) {
	int i;
    threadpool_t *pool = this;
//        if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
//            printf("malloc threadpool fail");
//            // 跳出do while
//            break;
//        }
    pool->min_thr_num = min_thr_num;
    pool->max_thr_num = max_thr_num;
    pool->busy_thr_num = 0;
    // 活着的线程数初值=最小线程数
    pool->live_thr_num = min_thr_num;
    // 有0个产品
    pool->queue_size = 0;
    pool->queue_max_size = queue_max_size;
    pool->queue_front = 0;
    pool->queue_rear = 0;
    // 不关闭线程池
    pool->shutdown = false;

    /* 根据最大线程上限数， 给工作线程数组开辟空间, 并清零 */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*max_thr_num);
    if (pool->threads == NULL) {
        printf("malloc threads fail");
        exit(-1);
    }
    memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);

    /* 队列开辟空间 */
    pool->task_queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t)*queue_max_size);
    if (pool->task_queue == NULL) {
        printf("malloc task_queue fail");
        exit(-1);
    }

    /* 初始化互斥琐、条件变量 */
    if (pthread_mutex_init(&(pool->lock), NULL) != 0
            || pthread_mutex_init(&(pool->thread_counter), NULL) != 0
            || pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
            || pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
    {
        printf("init the lock or cond fail");
        exit(-1);
    }
    // printf("min_work start create ---------- start\n");
    /* 启动 min_thr_num 个 work thread */
    for (i = 0; i < min_thr_num; i++) {
        /*
            * 第一个参数为指向线程标识符的指针。
　　		 * 第二个参数用来设置线程属性。
　　		 * 第三个参数是线程运行函数的地址。
　　		 * 最后一个参数是运行函数的参数。
            */
        // pool指向当前线程池
        pthread_create(&(pool->threads[i]), NULL, worker_thread, (void *)pool);
        printf("start thread 0x%x...\n", (unsigned int)pool->threads[i]);
    }
    // printf("min_work was created ----------  end\n");
    // printf("worker_adjust  ----------  start\n");
    // 启动管理者线程
    pthread_create(&(pool->adjust_tid), NULL, worker_adjust, (void *)pool);
    // printf("===== worker_thread working =====\n");
    printf("线程基本初始化完成\n");
}

// 向线程池中添加一个任务
template<typename T>
int threadpool_t<T>::
threadpool_add(void*(*function)(void *arg), void *arg) {
	// 申请结构体锁，此锁用于锁定结构体
    pthread_mutex_lock(&(this->lock));

    // ==为真，队列已经满，调wait阻塞
    while ((this->queue_size == this->queue_max_size) && (!this->shutdown)) {
        pthread_cond_wait(&(this->queue_not_full), &(this->lock));
    }
	// 如果线程池关闭，则释放锁
    if (this->shutdown) {
        pthread_mutex_unlock(&(this->lock));
    }

    // 由于是循环队列所以，先清空工作线程调用的回调函数的参数arg
    if (this->task_queue[this->queue_rear].arg != NULL) {
        free(this->task_queue[this->queue_rear].arg);
        this->task_queue[this->queue_rear].arg = NULL;
    }
    // 添加任务到任务队列里
    this->task_queue[this->queue_rear].function = function;
    this->task_queue[this->queue_rear].arg = arg;
	// 队尾指针移动, 模拟环形
    this->queue_rear = (this->queue_rear + 1) % this->queue_max_size;
    this->queue_size++;

    // 添加完任务后，队列不为空，唤醒线程池中等待处理任务的线程
    pthread_cond_signal(&(this->queue_not_empty));
    pthread_mutex_unlock(&(this->lock));

    return 0;
}

// 工作线程运行函数
template<typename T>
void* threadpool_t<T>::
worker_thread(void* arg) {
    // printf("===== worker_thread start=====\n");
    threadpool_t* pool = (threadpool_t* )arg;
    pool->threadpool_thread();
    // printf("===== worker_thread  end =====\n");
    return (void*)pool;
}

/* 线程池中各个工作线程 */
template<typename T>
void* threadpool_t<T>::
threadpool_thread() {
	/* 线程池结构体，其实就是操作传入的参数 */
//    threadpool_t *pool = (threadpool_t *)threadpool;
    /* 子线程任务的结构体 */
	threadpool_task_t task;

    while (true) {
        /* Lock must be taken to wait on conditional variable */
        /* 刚创建出线程，等待任务队列里有任务，否则阻塞等待任务队列里有任务后再唤醒接收任务 */
        pthread_mutex_lock(&(this->lock));

        /* queue_size == 0 说明没有任务，调 wait 阻塞在条件变量上, 若有任务，跳过该while */
        while ((this->queue_size == 0) && (!this->shutdown)) {
            printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
            pthread_cond_wait(&(this->queue_not_empty), &(this->lock));

            /* 清除指定数目的空闲线程，如果要结束的线程个数大于0，结束线程 */
            if (this->wait_exit_thr_num > 0) {
                this->wait_exit_thr_num--;

                /*如果线程池里线程个数大于最小值时可以结束当前线程*/
                if (this->live_thr_num > this->min_thr_num) {
                    printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
                    this->live_thr_num--;
                    pthread_mutex_unlock(&(this->lock));
                    pthread_exit(NULL);
                }
            }
        }

        /* 如果指定了true，要关闭线程池里的每个线程，自行退出处理 */
        if (this->shutdown) {
            pthread_mutex_unlock(&(this->lock));
            printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
            pthread_exit(NULL);     /* 线程自行结束 */
        }

        /* 从任务队列里获取任务, 是一个出队操作 */
        task.function = this->task_queue[this->queue_front].function;
        task.arg = this->task_queue[this->queue_front].arg;
		/* 出队，模拟环形队列 */
        this->queue_front = (this->queue_front + 1) % this->queue_max_size;
        this->queue_size--;

        /* 通知可以有新的任务添加进来 */
        pthread_cond_broadcast(&(this->queue_not_full));

        /* 任务取出后，立即将 线程池琐释放 */
        pthread_mutex_unlock(&(this->lock));

        /* 执行任务 */
        printf("thread 0x%x start working\n", (unsigned int)pthread_self());
        /* 忙状态线程数变量琐 */
		pthread_mutex_lock(&(this->thread_counter));
        /* 忙状态线程数+1 */
		this->busy_thr_num++;
        pthread_mutex_unlock(&(this->thread_counter));
		/* 执行回调函数任务 */
        (*(task.function))(task.arg);
        //task.function(task.arg);
		/* 执行回调函数任务 */
        /* 任务结束处理 */
        printf("thread 0x%x end working\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(this->thread_counter));
        this->busy_thr_num--;                                       /*处理掉一个任务，忙状态数线程数-1*/
        pthread_mutex_unlock(&(this->thread_counter));
    }

    pthread_exit(NULL);
}

// 管理者线程运行函数
template<typename T>
void* threadpool_t<T>::
worker_adjust(void* arg) {
    // printf("===== adjust_thread =====\n");
    threadpool_t* pool = (threadpool_t* )arg;
    pool->adjust_thread();
    // printf("===== adjust_thread =====\n");
    return (void*)pool;
}

/* 管理线程 */
template<typename T>
void* threadpool_t<T>::
adjust_thread() {
    // printf("===== adjust_thread start =====\n");
    int i;
//    threadpool_t *pool = (threadpool_t *)threadpool;
    while (!this->shutdown) {
		// 定时对线程池管理
        sleep(DEFAULT_TIME);
		// 获取线程池的信息，队列的大小，存活的线程数，工作的线程数
        pthread_mutex_lock(&(this->lock));
        // 关注任务数
        int queue_size = this->queue_size;
        // 存活线程数
		int live_thr_num = this->live_thr_num;
        pthread_mutex_unlock(&(this->lock));

        pthread_mutex_lock(&(this->thread_counter));
        // 忙着的线程数
        int busy_thr_num = this->busy_thr_num;
        pthread_mutex_unlock(&(this->thread_counter));

        // 创建新线程，算法：
		// 任务数大于最小线程池个数，且存活的线程数少于最大线程个数时
		// 如：30 >= 10 && 40 < 100
        if (queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < this->max_thr_num) {
            pthread_mutex_lock(&(this->lock));
            int add = 0;

            // 一次增加 DEFAULT_THREAD 个线程
            for (i = 0; i < this->max_thr_num && add < DEFAULT_THREAD_VARY
                    && this->live_thr_num < this->max_thr_num; i++) {
                if (this->threads[i] == 0 || !is_thread_alive(this->threads[i])) {
                    pthread_create(&(this->threads[i]), NULL, worker_thread, (void *)this);
                    add++;
                    this->live_thr_num++;
                }
            }
            pthread_mutex_unlock(&(this->lock));
        }

        // 销毁多余的空闲线程，算法
		// 忙线程X2 < 活的线程数 & 存活的线程数 > 最小线程数
        if ((busy_thr_num * 2) < live_thr_num  &&  live_thr_num > this->min_thr_num) {

            // 一次销毁DEFAULT_THREAD个线程, 随机10个即可
            pthread_mutex_lock(&(this->lock));
            // 要销毁的线程数 设置为10
            this->wait_exit_thr_num = DEFAULT_THREAD_VARY;
            pthread_mutex_unlock(&(this->lock));

            for (i = 0; i < DEFAULT_THREAD_VARY; i++) {
                // 通知处在空闲状态的线程，他们会自行终止
                pthread_cond_signal(&(this->queue_not_empty));
            }
        }
        this->threadpool_info();
    }

    return NULL;
}

template<typename T>
threadpool_t<T>::
~threadpool_t() {
	__threadpool_destroy(this);
    printf("thread_pool was destoried\n");
}

template<typename T>
int threadpool_t<T>::
__threadpool_destroy(threadpool_t *pool) {
    int i;
    if (pool == NULL) {
        return -1;
    }
    pool->shutdown = true;

    // 先销毁管理线程
    pthread_join(pool->adjust_tid, NULL);

    for (i = 0; i < pool->live_thr_num; i++) {
        // 通知所有的空闲线程
        pthread_cond_broadcast(&(pool->queue_not_empty));
    }
	// 回收线程
    for (i = 0; i < pool->live_thr_num; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    __threadpool_free(pool);

    return 0;
}

template<typename T>
int threadpool_t<T>::
__threadpool_free(threadpool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    if (pool->task_queue) {
        free(pool->task_queue);
    }
	// 将锁逐个的销毁
    if (pool->threads) {
        free(pool->threads);
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_mutex_lock(&(pool->thread_counter));
        pthread_mutex_destroy(&(pool->thread_counter));
        pthread_cond_destroy(&(pool->queue_not_empty));
        pthread_cond_destroy(&(pool->queue_not_full));
    }
    // free(pool);
    pool = NULL;

    return 0;
}

// 返回线程的总数
template<typename T>
int threadpool_t<T>::
threadpool_all_threadnum() {
    int all_threadnum = -1;
    pthread_mutex_lock(&(this->lock));
    all_threadnum = this->live_thr_num;
    pthread_mutex_unlock(&(this->lock));
    return all_threadnum;
}

// 返回工作线程的数目
template<typename T>
int threadpool_t<T>::
threadpool_busy_threadnum() {
    int busy_threadnum = -1;
    pthread_mutex_lock(&(this->thread_counter));
    busy_threadnum = this->busy_thr_num;
    pthread_mutex_unlock(&(this->thread_counter));
    return busy_threadnum;
}

// 返回工作线程的数目
template<typename T>
int threadpool_t<T>::
threadpool_queue_size() {
    pthread_mutex_lock(&(this->thread_counter));
    int res = this->queue_size;
    pthread_mutex_unlock(&(this->thread_counter));
    return res;
}

// 判断某个线程是否存活
template<typename T>
bool threadpool_t<T>::
is_thread_alive(pthread_t tid) {
	// 发0号信号，测试线程是否存活
    int kill_rc = pthread_kill(tid, 0);
    if (kill_rc == ESRCH) {
        return false;
    }
    return true;
}

template<typename T>
void threadpool_t<T>::
threadpool_info() {
	printf("=========== THREAD INFO START ===========\n");
	printf("线程池最小线程数：%d\n", this->min_thr_num);
	printf("线程池最大线程数：%d\n", this->max_thr_num);
    printf("工作队列中的数据：%d\n", this->threadpool_queue_size());
	printf("当前存活线程个数：%d\n", threadpool_all_threadnum());
	printf("忙状态的线程个数：%d\n", threadpool_busy_threadnum());
	printf("=========== THREAD INFO  END  ===========\n");
}

/*测试*/

#if 1
/* 线程池中的线程，模拟处理业务 */
void *process(void *arg)
{
    // printf("thread 0x%x working on task %d\n ",(unsigned int)pthread_self(),*(int *)arg);
    sleep(2);
    printf("task %d is end\n",*(int *)arg);

    return NULL;
}

int main(void)
{
    // 查看线程池的析构函数
	{
        // 创建线程池，池里最小3个线程，最大100，队列最大100
	    threadpool_t<threadpool_task_t> *pool = new threadpool_t<threadpool_task_t>(3,100,100);

		printf("pool inited\n");
		pool->threadpool_info();
	    //int *num = (int *)malloc(sizeof(int)*20);
	    int num[20];
	    for (int i = 0; i < 30; i++) {
	        num[i] = i;
	        printf("add task %d\n",i);
	        // 向线程池中添加任务
	        pool->threadpool_add(process, (void*)&num[i]);
            // pool->threadpool_info();
	    }
	    sleep(20);
        printf("everything was finished! Now destory the thread_pool!\n");
	    pool->threadpool_info();
        delete pool;
	}
	// sleep(2);
    return 0;
}

#endif
#endif