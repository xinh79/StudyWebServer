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

// 10s���һ��
#define DEFAULT_TIME 3
// ���queue_size > MIN_WAIT_TASK_NUM ����µ��̵߳��̳߳�
#define MIN_WAIT_TASK_NUM 10
// ÿ�δ����������̵߳ĸ���
#define DEFAULT_THREAD_VARY 10

// �����߳�����ṹ��
typedef struct {
	// ����ָ�룬�ص�����
    void *(*function)(void *);
    // ���溯���Ĳ���
    void *arg;
} threadpool_task_t;

// �����̳߳������Ϣ
template<typename T>
class threadpool_t {
public:
	// ���캯������Ҫ������С���߳����������߳��������е����ȴ�����
	threadpool_t(int min_thr_num, int max_thr_num, int queue_max_size);
	~threadpool_t();
	// �������߳�
	static void *worker_thread(void* arg);
	void *threadpool_thread();
	// �������߳�
	static void *worker_adjust(void* arg);
	void *adjust_thread();

	// ���ĳһ���߳��Ƿ����
	static bool is_thread_alive(pthread_t tid);
	// ���̳߳������һ������
	int threadpool_add(void*(*function)(void *arg), void *arg);
	// �����ܵ��߳���Ŀ
	int threadpool_all_threadnum();
	// ���ع����߳���Ŀ
	int threadpool_busy_threadnum();
    // ���ع������д�С
    int threadpool_queue_size();
	// ��ӡ�̳߳ص���Ϣ
	void threadpool_info();

    void shutdown_pool() {
        this->shutdown = true;
    }

private:
	// ������ס���ṹ��
    pthread_mutex_t lock;
	// ��¼æ״̬�̸߳�����������busy_thr_num
    pthread_mutex_t thread_counter;
	// �����������ʱ�����������߳��������ȴ�����������
    pthread_cond_t queue_not_full;
	// ��������ﲻΪ��ʱ��֪ͨ�ȴ�������߳�
    pthread_cond_t queue_not_empty;

	// ����̳߳���ÿ���̵߳�tid������
    pthread_t *threads;
	// ������߳�tid
    pthread_t adjust_tid;
	// �������
	// std::list<T*> workqueue;
    threadpool_task_t *task_queue;

	// �̳߳���С�߳���
    int min_thr_num;
	// �̳߳�����߳���
    int max_thr_num;
	// ��ǰ����̸߳���
    int live_thr_num;
	// æ״̬���̸߳���
    int busy_thr_num;
	// Ҫ���ٵ��̸߳���
    int wait_exit_thr_num;

    int queue_front;                    /* task_queue��ͷ�±� */
    int queue_rear;                     /* task_queue��β�±� */
    int queue_size;                     /* task_queue����ʵ�������� */
    int queue_max_size;                 /* task_queue���п��������������� */

	// ��־λ���̳߳�ʹ��״̬��true��false
    bool shutdown;
    // �����ڴ�ص���Դ
	int __threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);
    // �ͷ��ڴ�ص���Դ
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

// �����̳߳��������
template<typename T>
int threadpool_t<T>::
__threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size) {
	int i;
    threadpool_t *pool = this;
//        if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
//            printf("malloc threadpool fail");
//            // ����do while
//            break;
//        }
    pool->min_thr_num = min_thr_num;
    pool->max_thr_num = max_thr_num;
    pool->busy_thr_num = 0;
    // ���ŵ��߳�����ֵ=��С�߳���
    pool->live_thr_num = min_thr_num;
    // ��0����Ʒ
    pool->queue_size = 0;
    pool->queue_max_size = queue_max_size;
    pool->queue_front = 0;
    pool->queue_rear = 0;
    // ���ر��̳߳�
    pool->shutdown = false;

    /* ��������߳��������� �������߳����鿪�ٿռ�, ������ */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t)*max_thr_num);
    if (pool->threads == NULL) {
        printf("malloc threads fail");
        exit(-1);
    }
    memset(pool->threads, 0, sizeof(pthread_t)*max_thr_num);

    /* ���п��ٿռ� */
    pool->task_queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t)*queue_max_size);
    if (pool->task_queue == NULL) {
        printf("malloc task_queue fail");
        exit(-1);
    }

    /* ��ʼ������������������ */
    if (pthread_mutex_init(&(pool->lock), NULL) != 0
            || pthread_mutex_init(&(pool->thread_counter), NULL) != 0
            || pthread_cond_init(&(pool->queue_not_empty), NULL) != 0
            || pthread_cond_init(&(pool->queue_not_full), NULL) != 0)
    {
        printf("init the lock or cond fail");
        exit(-1);
    }
    // printf("min_work start create ---------- start\n");
    /* ���� min_thr_num �� work thread */
    for (i = 0; i < min_thr_num; i++) {
        /*
            * ��һ������Ϊָ���̱߳�ʶ����ָ�롣
����		 * �ڶ����������������߳����ԡ�
����		 * �������������߳����к����ĵ�ַ��
����		 * ���һ�����������к����Ĳ�����
            */
        // poolָ��ǰ�̳߳�
        pthread_create(&(pool->threads[i]), NULL, worker_thread, (void *)pool);
        printf("start thread 0x%x...\n", (unsigned int)pool->threads[i]);
    }
    // printf("min_work was created ----------  end\n");
    // printf("worker_adjust  ----------  start\n");
    // �����������߳�
    pthread_create(&(pool->adjust_tid), NULL, worker_adjust, (void *)pool);
    // printf("===== worker_thread working =====\n");
    printf("�̻߳�����ʼ�����\n");
}

// ���̳߳������һ������
template<typename T>
int threadpool_t<T>::
threadpool_add(void*(*function)(void *arg), void *arg) {
	// ����ṹ�������������������ṹ��
    pthread_mutex_lock(&(this->lock));

    // ==Ϊ�棬�����Ѿ�������wait����
    while ((this->queue_size == this->queue_max_size) && (!this->shutdown)) {
        pthread_cond_wait(&(this->queue_not_full), &(this->lock));
    }
	// ����̳߳عرգ����ͷ���
    if (this->shutdown) {
        pthread_mutex_unlock(&(this->lock));
    }

    // ������ѭ���������ԣ�����չ����̵߳��õĻص������Ĳ���arg
    if (this->task_queue[this->queue_rear].arg != NULL) {
        free(this->task_queue[this->queue_rear].arg);
        this->task_queue[this->queue_rear].arg = NULL;
    }
    // ����������������
    this->task_queue[this->queue_rear].function = function;
    this->task_queue[this->queue_rear].arg = arg;
	// ��βָ���ƶ�, ģ�⻷��
    this->queue_rear = (this->queue_rear + 1) % this->queue_max_size;
    this->queue_size++;

    // ���������󣬶��в�Ϊ�գ������̳߳��еȴ�����������߳�
    pthread_cond_signal(&(this->queue_not_empty));
    pthread_mutex_unlock(&(this->lock));

    return 0;
}

// �����߳����к���
template<typename T>
void* threadpool_t<T>::
worker_thread(void* arg) {
    // printf("===== worker_thread start=====\n");
    threadpool_t* pool = (threadpool_t* )arg;
    pool->threadpool_thread();
    // printf("===== worker_thread  end =====\n");
    return (void*)pool;
}

/* �̳߳��и��������߳� */
template<typename T>
void* threadpool_t<T>::
threadpool_thread() {
	/* �̳߳ؽṹ�壬��ʵ���ǲ�������Ĳ��� */
//    threadpool_t *pool = (threadpool_t *)threadpool;
    /* ���߳�����Ľṹ�� */
	threadpool_task_t task;

    while (true) {
        /* Lock must be taken to wait on conditional variable */
        /* �մ������̣߳��ȴ���������������񣬷��������ȴ������������������ٻ��ѽ������� */
        pthread_mutex_lock(&(this->lock));

        /* queue_size == 0 ˵��û�����񣬵� wait ����������������, ��������������while */
        while ((this->queue_size == 0) && (!this->shutdown)) {
            printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
            pthread_cond_wait(&(this->queue_not_empty), &(this->lock));

            /* ���ָ����Ŀ�Ŀ����̣߳����Ҫ�������̸߳�������0�������߳� */
            if (this->wait_exit_thr_num > 0) {
                this->wait_exit_thr_num--;

                /*����̳߳����̸߳���������Сֵʱ���Խ�����ǰ�߳�*/
                if (this->live_thr_num > this->min_thr_num) {
                    printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
                    this->live_thr_num--;
                    pthread_mutex_unlock(&(this->lock));
                    pthread_exit(NULL);
                }
            }
        }

        /* ���ָ����true��Ҫ�ر��̳߳����ÿ���̣߳������˳����� */
        if (this->shutdown) {
            pthread_mutex_unlock(&(this->lock));
            printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
            pthread_exit(NULL);     /* �߳����н��� */
        }

        /* ������������ȡ����, ��һ�����Ӳ��� */
        task.function = this->task_queue[this->queue_front].function;
        task.arg = this->task_queue[this->queue_front].arg;
		/* ���ӣ�ģ�⻷�ζ��� */
        this->queue_front = (this->queue_front + 1) % this->queue_max_size;
        this->queue_size--;

        /* ֪ͨ�������µ�������ӽ��� */
        pthread_cond_broadcast(&(this->queue_not_full));

        /* ����ȡ���������� �̳߳����ͷ� */
        pthread_mutex_unlock(&(this->lock));

        /* ִ������ */
        printf("thread 0x%x start working\n", (unsigned int)pthread_self());
        /* æ״̬�߳��������� */
		pthread_mutex_lock(&(this->thread_counter));
        /* æ״̬�߳���+1 */
		this->busy_thr_num++;
        pthread_mutex_unlock(&(this->thread_counter));
		/* ִ�лص��������� */
        (*(task.function))(task.arg);
        //task.function(task.arg);
		/* ִ�лص��������� */
        /* ����������� */
        printf("thread 0x%x end working\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(this->thread_counter));
        this->busy_thr_num--;                                       /*�����һ������æ״̬���߳���-1*/
        pthread_mutex_unlock(&(this->thread_counter));
    }

    pthread_exit(NULL);
}

// �������߳����к���
template<typename T>
void* threadpool_t<T>::
worker_adjust(void* arg) {
    // printf("===== adjust_thread =====\n");
    threadpool_t* pool = (threadpool_t* )arg;
    pool->adjust_thread();
    // printf("===== adjust_thread =====\n");
    return (void*)pool;
}

/* �����߳� */
template<typename T>
void* threadpool_t<T>::
adjust_thread() {
    // printf("===== adjust_thread start =====\n");
    int i;
//    threadpool_t *pool = (threadpool_t *)threadpool;
    while (!this->shutdown) {
		// ��ʱ���̳߳ع���
        sleep(DEFAULT_TIME);
		// ��ȡ�̳߳ص���Ϣ�����еĴ�С�������߳������������߳���
        pthread_mutex_lock(&(this->lock));
        // ��ע������
        int queue_size = this->queue_size;
        // ����߳���
		int live_thr_num = this->live_thr_num;
        pthread_mutex_unlock(&(this->lock));

        pthread_mutex_lock(&(this->thread_counter));
        // æ�ŵ��߳���
        int busy_thr_num = this->busy_thr_num;
        pthread_mutex_unlock(&(this->thread_counter));

        // �������̣߳��㷨��
		// ������������С�̳߳ظ������Ҵ����߳�����������̸߳���ʱ
		// �磺30 >= 10 && 40 < 100
        if (queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < this->max_thr_num) {
            pthread_mutex_lock(&(this->lock));
            int add = 0;

            // һ������ DEFAULT_THREAD ���߳�
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

        // ���ٶ���Ŀ����̣߳��㷨
		// æ�߳�X2 < ����߳��� & �����߳��� > ��С�߳���
        if ((busy_thr_num * 2) < live_thr_num  &&  live_thr_num > this->min_thr_num) {

            // һ������DEFAULT_THREAD���߳�, ���10������
            pthread_mutex_lock(&(this->lock));
            // Ҫ���ٵ��߳��� ����Ϊ10
            this->wait_exit_thr_num = DEFAULT_THREAD_VARY;
            pthread_mutex_unlock(&(this->lock));

            for (i = 0; i < DEFAULT_THREAD_VARY; i++) {
                // ֪ͨ���ڿ���״̬���̣߳����ǻ�������ֹ
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

    // �����ٹ����߳�
    pthread_join(pool->adjust_tid, NULL);

    for (i = 0; i < pool->live_thr_num; i++) {
        // ֪ͨ���еĿ����߳�
        pthread_cond_broadcast(&(pool->queue_not_empty));
    }
	// �����߳�
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
	// �������������
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

// �����̵߳�����
template<typename T>
int threadpool_t<T>::
threadpool_all_threadnum() {
    int all_threadnum = -1;
    pthread_mutex_lock(&(this->lock));
    all_threadnum = this->live_thr_num;
    pthread_mutex_unlock(&(this->lock));
    return all_threadnum;
}

// ���ع����̵߳���Ŀ
template<typename T>
int threadpool_t<T>::
threadpool_busy_threadnum() {
    int busy_threadnum = -1;
    pthread_mutex_lock(&(this->thread_counter));
    busy_threadnum = this->busy_thr_num;
    pthread_mutex_unlock(&(this->thread_counter));
    return busy_threadnum;
}

// ���ع����̵߳���Ŀ
template<typename T>
int threadpool_t<T>::
threadpool_queue_size() {
    pthread_mutex_lock(&(this->thread_counter));
    int res = this->queue_size;
    pthread_mutex_unlock(&(this->thread_counter));
    return res;
}

// �ж�ĳ���߳��Ƿ���
template<typename T>
bool threadpool_t<T>::
is_thread_alive(pthread_t tid) {
	// ��0���źţ������߳��Ƿ���
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
	printf("�̳߳���С�߳�����%d\n", this->min_thr_num);
	printf("�̳߳�����߳�����%d\n", this->max_thr_num);
    printf("���������е����ݣ�%d\n", this->threadpool_queue_size());
	printf("��ǰ����̸߳�����%d\n", threadpool_all_threadnum());
	printf("æ״̬���̸߳�����%d\n", threadpool_busy_threadnum());
	printf("=========== THREAD INFO  END  ===========\n");
}

/*����*/

#if 0
/* �̳߳��е��̣߳�ģ�⴦��ҵ�� */
void *process(void *arg)
{
    // printf("thread 0x%x working on task %d\n ",(unsigned int)pthread_self(),*(int *)arg);
    sleep(2);
    printf("task %d is end\n",*(int *)arg);

    return NULL;
}

int main(void)
{
    // �鿴�̳߳ص���������
	{
        // �����̳߳أ�������С3���̣߳����100���������100
	    threadpool_t<threadpool_task_t> *pool = new threadpool_t<threadpool_task_t>(3,100,100);

		printf("pool inited\n");
		pool->threadpool_info();
	    //int *num = (int *)malloc(sizeof(int)*20);
	    int num[20];
	    for (int i = 0; i < 30; i++) {
	        num[i] = i;
	        printf("add task %d\n",i);
	        // ���̳߳����������
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
