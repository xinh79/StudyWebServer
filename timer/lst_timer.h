#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include "../log/log.h"

#define BUFFER_SIZE 64 // 缓冲区大小

class util_timer; //前项申明

// 用户节点
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];//读取缓存
    util_timer *timer;//定时器
};

// 定时器类中的双向链表节点
// 用来初始化每个节点
// 里面包含成员client_data的指针
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;                  // 过期时间
    void (*cb_func)(client_data *); // 回调函数指针
    client_data *user_data;         // 用户数据指针
    util_timer *prev;               // 前向指针
    util_timer *next;               // 后向指针
};

// 对外提供的定时器操作的函数类
// 本质是基于排序链表实现
class sort_timer_lst
{
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}

    ~sort_timer_lst()
    {
        util_timer *tmp = head; // 析构函数，依次删除链表中的定时器
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    /*
     * 往双向链表中添加定时器
     * @timer 双向链表节点
     */
    void add_timer(util_timer *timer)
    {
        // 参数无效
        if (!timer)
        {
            return;
        }
        // 表头为空，则用此定时器初始化头尾指针
        if (!head)
        {
            head = tail = timer;
            return;
        }
        // 基于排序链表构造方式插入节点
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }
    /*
     * 调整指针的定时器节点，更新时间后，需要重新插入到定时器链表中
     * @timer 待更新位置的定时器节点
     */
    void adjust_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        util_timer *tmp = timer->next;
        if (!tmp || (timer->expire < tmp->expire))
        {
            // 最后一个节点或者是之后的过期时间比他更晚
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }
    /*
     * 删除指定的定时器节点
     * @timer 指定的过期定时器
     */
    void del_timer(util_timer *timer)
    {
        // 定时器链表为空
        if (!timer)
        {
            return;
        }
        // 只要一个节点，此节点为过期节点
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        // 过期节点为头结点
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        // 过期节点为尾结点
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        // 排除特殊情况后，删除节点，改变头尾指针即可
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
    /*
    * SIGALRM信号每次被触发就在其信号处理函数(如果使用同一事件源，则是主函数)中执行一次tick函数，
    * 以处理链表上到期的任务下文会给出使用SIGALRM测试的代码，当然也可以使用其他方式
    * 顺序检查链表中的定时器是否过期
    * 如果过期则删除节点
    */
    void tick()
    {
        if (!head)
        {
            return;
        }
        //printf( "timer tick\n" );
        LOG_INFO("%s", "timer tick"); // 写入定期器日志文件
        Log::get_instance()->flush(); // 手动刷新缓冲区
        time_t cur = time(NULL);      // 返回值仍然是从1970年1月1日至今所经历的时间(以秒为单位)
        util_timer *tmp = head;       // 获取链表头部节点
        while (tmp)                   // 依次从头结点开始判断定时器是否过期
        {
            if (cur < tmp->expire)
            {
                break; // 头部节点不过期，由于是排序的链表，所以后面的节点肯定不过期
            }
            tmp->cb_func(tmp->user_data); // 过期的节点调用回调函数，处理过期事件
            head = tmp->next;             // 移动头结点位置
            if (head)                     // 头结点不为空，将头结点的前驱指针值为NULL
            {
                head->prev = NULL;
            }
            delete tmp; // 删除过期节点
            tmp = head; // 循环判断下一个节点是否过期
        }
    }

private:
    /*
    * add_timer重载函数
    * 找到第一个比timer更晚过期的节点位置，在他之前插入
    * @timer 待插入节点
    * @lst_head 等待插入队列的头结点
    */
    void add_timer(util_timer *timer, util_timer *lst_head)
    {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        while (tmp)
        {
            // 顺序寻找timer，找到第一个比他更晚过期的节点
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            // 往后移动查找节点
            prev = tmp;
            tmp = tmp->next;
        }
        // 为空，循环结束，则说明应该作为最后一个节点插入
        if (!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    util_timer *head;
    util_timer *tail;
};

#endif
