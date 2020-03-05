#include "http_conn.h"
#include "../log/log.h"
#include <map>
#include <mysql/mysql.h>
#include <sys/uio.h>

// 定义http响应的一些状态信息
// 200（成功）服务器已成功处理了请求。通常，这表示服务器提供了请求的网页
const char *ok_200_title = "OK";
// 400（错误请求）服务器不理解请求的语法
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
// 403（禁止）服务器拒绝请求
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
// 404（未找到）服务器找不到请求的网页
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
// 500（服务器内部错误）服务器遇到错误，无法完成请求
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
// 设置一个文件资源访问的根目录
const char *doc_root = "/home/h/StudyWebServer/root";

// 创建数据库连接池
connection_pool *connPool = connection_pool::GetInstance("localhost", "root", "1", "xinh", 3306, 5);

// 将表中的用户名和密码放入map
map<string, string> users;

/**
 * @desp: 获取数据库连接池，然后连接数据库获取数据，保存用户信息与密码，初始化users
 * @param {type} 无
 * @return: 无
 */
void http_conn::initmysql_result()
{
    MYSQL *mysql = connPool->GetConnection();                   // 先从连接池中取一个连接
    if (mysql_query(mysql, "SELECT username,passwd FROM user")) // 在user表中检索username，passwd数据，浏览器端输入
    {                                                           //
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));     // 将查询错误在日志中纪录
    }                                                           //
    MYSQL_RES *result = mysql_store_result(mysql);              // 从表中检索完整的结果集
    int num_fields = mysql_num_fields(result);                  // 返回结果集中的列数
    MYSQL_FIELD *fields = mysql_fetch_fields(result);           // 返回所有字段结构的数组
    while (MYSQL_ROW row = mysql_fetch_row(result))             // 从结果集中获取下一行，将对应的用户名和密码，存入map中
    {                                                           //
        string temp1(row[0]);                                   // 用户名信息
        string temp2(row[1]);                                   // 密码信息
        users[temp1] = temp2;                                   // 初始化用户名和密码的map
    }                                                           //
    connPool->ReleaseConnection(mysql);                         // 将连接归还连接池
}

/**
 * @desp: 对文件描述符设置非阻塞
 * @param fd 待设置的文件描述符
 * @return: fcntl的返回值，失败返回-1
 */
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);      // 常规操作，先获取文件描述符
    int new_option = old_option | O_NONBLOCK; // 保持原有文件描述符的属性后，新增非阻塞属性
    fcntl(fd, F_SETFL, new_option);           // 设置新的属性
    return old_option;                        // 返回最初执行的情况
}

/**
 * @desp: 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
 * EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，
 * 如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
 * @param epollfd epoll文件描述；fd 新增的文件描述符；
 * @return: 无
 */
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;              // 如果为真，则说明要设置为EPOLLONESHOT
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); // 将节点加入epoll树上
    setnonblocking(fd);                            // 将文件描述符设置为非阻塞
}

/**
 * @desp: 从内核时间表删除描述符
 * @param epollfd epoll 树的文件描述符；fd 待删除的文件描述符
 * @return: 无
 */
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0); // 从epoll树上摘下节点
    close(fd);                                // 删除后关闭文件描述符，释放资源
}

/**
 * @desp: 将事件重置为EPOLLONESHOT
 * @param epollfd epoll 树的文件描述符；fd 待修改的文件描述符；ev 为对应的事件如EPOLLIN/EPOLLOUT
 * @return: 无
 */
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;                                      // 初始化文件描述符
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP; // 设置边沿，一次触发，读端关闭
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);           // 设置MOD属性，然后修改epoll树上对应的节点
}

int http_conn::m_user_count = 0; // 初始化用户数量
int http_conn::m_epollfd = -1;   // 初始化epoll树的文件描述符

/**
 * @desp: 关闭连接，关闭一个连接，客户总量减一
 * @param real_close
 * @return: 无
 */
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd); // 当前连接的client sockdfd，将其从树上摘下
        m_sockfd = -1;                 // 重置文件描述符
        m_user_count--;                // 用户数量减一
    }
}

/**
 * @desp: 初始化连接，私有成员，只能自己调用
 * @param sockfd 文件描述符，addr 结构体信息
 * @return: 无
 */
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;              // 传入自己的文件描述符
    m_address = addr;               // 自己的结构体信息
    addfd(m_epollfd, sockfd, true); // 加入epoll数中，同时选择开启EPOLLONESHOT
    m_user_count++;                 // 用户数加一
    init();                         // 继续完成初始化，初始化新接收的连接
}

/**
 * @desp: 初始化新接受的连接，check_state默认为分析请求行状态
 * @param 无
 * @return: 无
 */
void http_conn::init()
{
    m_check_state = CHECK_STATE_REQUESTLINE; // 主状态机的状态
    m_linger = false;                        // http请求是否要保持链接
    m_method = GET;                          // 请求的方法
    m_url = 0;                               // http请求是否要保持链接
    m_version = 0;                           // http版本信息，我们只支持1.1
    m_content_length = 0;                    // http请求的消息体的长度
    m_host = 0;                              // 主机信息
    m_start_line = 0;                        // 当前正在解析的行的起始位置
    m_checked_idx = 0;                       // 当前正在分析的字符在读缓冲区的位置
    m_read_idx = 0;                          // 标示读缓冲区已经读入的客户数据的最后一个字节的下一个位置
    m_write_idx = 0;                         // 写缓冲区中待发送的字节数
    cgi = 0;                                 // 是否启用的POST
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

/**
 * @desp: 从状态机，用于分析出一行内容
 * @param 无
 * @return: 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
 */
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/**
 * @desp: 循环读取客户数据，直到无数据可读或对方关闭连接
 * 非阻塞ET工作模式下，需要一次性将数据读完
 * @param 无
 * @return: 读取成功返回true，失败返回false
 */
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false; // 读取的下一个字节位置超过缓冲区的大小，为了避免越界，直接返回false
    }
    int bytes_read = 0;
    while (true) // 一直读取缓冲区的数据，知道无数据可读
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            //EAGAIN 11 Try again || EWOULDBLOCK EAGAIN Operation would block
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read; // 把已读的字节数累加
    }
    return true;
}

/**
 * @desp: 解析http请求行，获得请求方法，目标url及http版本号
 * @param text 客户端发送来的数据
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t"); // strpbrk获取第一个匹配字符串

    if (!m_url)
    {
        return BAD_REQUEST; // url为空
    }

    *m_url++ = '\0';

    char *method = text; // 获取客户端发送的信息

    if (strcasecmp(method, "GET") == 0) // strcasecmp()，比较时会自动忽略大小写的差异
        m_method = GET;                 // 获取资源
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST; // POST方式
        cgi = 1;         // 启动CGI，对数据库进行操作
    }
    else
        return BAD_REQUEST; // 方式不明确

    m_url += strspn(m_url, " \t"); // 第一个不在字符串中出现的下标
    m_version = strpbrk(m_url, " \t");

    if (!m_version)
        return BAD_REQUEST;

    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    if (strlen(m_url) == 1)          //当url为"/"时，显示判断界面
        strcat(m_url, "judge.html"); // 0是注册，1是登录

    m_check_state = CHECK_STATE_HEADER; // 更改处理的状态

    return NO_REQUEST;
}

/**
 * @desp: 解析http请求头信息，http的一个头部信息
 * @param text 客户端发送的数据信息
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

/**
 * @desp: 判断http请求是否被完整读入
 * @param text 客户端发送的数据信息
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/**
 * @desp: 处理请求数据
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    //printf("=========请求行头=========\n");
    //LOG_INFO("=========请求行头=========\n");
    //Log::get_instance()->flush();
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx; // 从当前正在分析的字符在读缓冲区的位置开始读取数据
        LOG_INFO("%s", text);         // 将数据写入日志
        Log::get_instance()->flush(); // 刷新缓冲区立马写入数据
        switch (m_check_state)        // 根据主状态机的状态判断执行
        {
        case CHECK_STATE_REQUESTLINE:       // 检查请求行
        {                                   //
            ret = parse_request_line(text); // 解析http请求行，获得请求方法，目标url及http版本号
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:       // 检查请求头部
        {                              //
            ret = parse_headers(text); // 解析http请求头信息，http的一个头部信息
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:      // 检查内容实体
        {                              //
            ret = parse_content(text); // 判断http请求是否被完整读入
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    //printf("=========请求行end=========\n");
    return NO_REQUEST;
}

/**
 * @desp: 对http请求发出响应，包括页面请求，用户登录，用户注册
 * @param {type} 无
 * @return: http响应报文的状态码
 */
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root); // 定为到页面访问的根目录
    int len = strlen(doc_root);
    //printf("m_url:%s\n", m_url);
    const char *p = strrchr(m_url, '/');
    //#if 0
    //处理cgi
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        char flag = m_url[1]; // 根据标志判断是登录检测还是注册检测

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);
        //================================================================================================
        //将用户名和密码提取出来
        //user=123&passwd=123
        // printf("请求头数据：%s\n", m_string);
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';
        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';
        // printf("user:%s & password:%s\n", name, password);
        //================================================================================================
        //同步线程登录校验
        //#if 0
        pthread_mutex_t lock;
        pthread_mutex_init(&lock, NULL);

        //从连接池中取一个连接
        MYSQL *mysql = connPool->GetConnection();

        //如果是注册，先检测数据库中是否有重名的
        //没有重名的，进行增加数据
        char *sql_insert = (char *)malloc(sizeof(char) * 200);
        strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, name);
        strcat(sql_insert, "', '");
        strcat(sql_insert, password);
        strcat(sql_insert, "')");
        // 3为注册
        if (*(p + 1) == '3')
        {
            if (users.find(name) == users.end())
            {
                pthread_mutex_lock(&lock);
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                pthread_mutex_unlock(&lock);

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
        connPool->ReleaseConnection(mysql);
//#endif

//CGI多进程登录校验
#if 0
        //fd[0]:读管道，fd[1]:写管道
        pid_t pid;
        int pipefd[2];
        if (pipe(pipefd) < 0)
        {
            LOG_ERROR("pipe() error:%d", 4);
            return BAD_REQUEST;
        }
        if ((pid = fork()) < 0)
        {
            LOG_ERROR("fork() error:%d", 3);
            return BAD_REQUEST;
        }

        if (pid == 0)
        {
            //标准输出，文件描述符是1，然后将输出重定向到管道写端
            dup2(pipefd[1], 1);
            //关闭管道的读端
            close(pipefd[0]);
            //父进程去执行cgi程序，m_real_file,name,password为输入
            //./check.cgi name password

            execl(m_real_file, &flag, name, password, NULL);
        }
        else
        {
            //printf("子进程\n");
            //子进程关闭写端，打开读端，读取父进程的输出
            close(pipefd[1]);
            char result;
            int ret = read(pipefd[0], &result, 1);

            if (ret != 1)
            {
                LOG_ERROR("管道read error:ret=%d", ret);
                return BAD_REQUEST;
            }
            if (flag == '2')
            {
                //printf("登录检测\n");
                LOG_INFO("%s", "登录检测");
                Log::get_instance()->flush();
                //当用户名和密码正确，则显示welcome界面，否则显示错误界面
                if (result == '1')
                    strcpy(m_url, "/welcome.html");
                //m_url="/welcome.html";
                else
                    strcpy(m_url, "/logError.html");
                //m_url="/logError.html";
            }
            else if (flag == '3')
            {
                //printf("注册检测\n");
                LOG_INFO("%s", "注册检测");
                Log::get_instance()->flush();
                //当成功注册后，则显示登陆界面，否则显示错误界面
                if (result == '1')
                    strcpy(m_url, "/log.html");
                //m_url="/log.html";
                else
                    strcpy(m_url, "/registerError.html");
                //m_url="/registerError.html";
            }
            //printf("m_url:%s\n", m_url);
            //回收进程资源
            waitpid(pid, NULL, 0);
            //waitpid(pid,0,NULL);
            //printf("回收完成\n");
        }
#endif
    }

    if (*(p + 1) == '0') // 0 为注册页面请求
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '1') // 1 为登录页面请求
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);

    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/**
 * @desp: 解除文件内存映射
 * @param {type} 无
 * @return: 无
 */
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
/**
 * @desp: 发送数据
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }
    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        //printf("temp:%d\n",temp);
        if (temp <= -1)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_to_send <= bytes_have_send)
        {
            unmap();
            if (m_linger)
            {
                //printf("========================\n");
                //printf("%s\n", "发送响应成功");
                init();                              // http请求是否要保持链接，则重新初始化
                modfd(m_epollfd, m_sockfd, EPOLLIN); // 继续挂在epoll树上监听文件描述符
                return true;
            }
            else
            {
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

/**
 * @desp: 添加响应，可变参数，用于各种报文拼接
 * @param 可变参数
 * @return: 成功为true，否则为false
 */
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    // _vsnprintf是C语言库函数之一，属于可变参数。用于向字符串中打印数据、数据格式用户自定义
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
        return false;
    m_write_idx += len;
    va_end(arg_list);
    //printf("%s\n",m_write_buf);
    LOG_INFO("request:%s", m_write_buf);
    Log::get_instance()->flush();
    return true;
}

/**
 * @desp: 添加响应状态行
 * @param status 状态 title 头
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

/**
 * @desp: 添加响应状态头
 * @param content_len 头长度
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_headers(int content_len)
{
    //add_content_type();
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

/**
 * @desp: 添加内容实体的长度-Content-Length字段
 * @param content_len 内容长度
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}

/**
 * @desp: 添加内容类型-Content-Type字段
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}

/**
 * @desp: 添加响应报文的Connection字段，指定是否保持连接
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

/**
 * @desp: 添加响应报文的空行-添加\r\n字段
 * @param {type} 无
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

/**
 * @desp: 添加响应内容
 * @param content 根据状态码确定响应内容
 * @return: 成功返回true，失败返回false
 */
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

/**
 * @desp: 处理写请求
 * @param ret http响应报文的状态码
 * @return: 成功返回true，失败返回false
 */
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR: // 500（服务器内部错误）服务器遇到错误，无法完成请求
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST: // 404（未找到）服务器找不到请求的网页
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST: // 403（禁止）服务器拒绝请求
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST: // 200（成功）服务器已成功处理了请求。通常，这表示服务器提供了请求的网页
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

/**
 * @desp: 处理http请求的线程调用的入口函数-回调函数
 * @param {type} 无
 * @return: 无
 */
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}
