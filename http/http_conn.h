#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include "../locker/locker.h"
#include "../mysql/sql_connection_pool.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;       // 文件名称长度
    static const int READ_BUFFER_SIZE = 2048;  // 读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024; // 写缓冲区大小
    enum METHOD                                // http请求的格式，只支持get
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE                 // 主状态机的状态，检查请求行，检查请求头部，检查内容实体
    {                                //
        CHECK_STATE_REQUESTLINE = 0, // 检查请求行
        CHECK_STATE_HEADER,          // 检查请求头部
        CHECK_STATE_CONTENT          // 检查内容实体
    };                               //
    enum HTTP_CODE                   // http响应报文的状态码
    {                                //
        NO_REQUEST,                  //
        GET_REQUEST,                 //
        BAD_REQUEST,                 // 400（错误请求）服务器不理解请求的语法
        NO_RESOURCE,                 //
        FORBIDDEN_REQUEST,           // 403（禁止）服务器拒绝请求
        FILE_REQUEST,                // 200（成功）服务器已成功处理了请求。通常，这表示服务器提供了请求的网页
        INTERNAL_ERROR,              // 500（服务器内部错误）服务器遇到错误，无法完成请求
        CLOSED_CONNECTION            //
    };                               //
    enum LINE_STATUS                 // 请求行的状态
    {                                //
        LINE_OK = 0,                 //
        LINE_BAD,                    //
        LINE_OPEN                    //
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result();

private:
    void init();                                            // 类的基本数据初始化
    HTTP_CODE process_read();                               // 处理请求数据
    bool process_write(HTTP_CODE ret);                      //
    HTTP_CODE parse_request_line(char *text);               // 解析http请求行，获得请求方法，目标url及http版本号
    HTTP_CODE parse_headers(char *text);                    // 解析http请求头信息，http的一个头部信息
    HTTP_CODE parse_content(char *text);                    // 判断http请求是否被完整读入
    HTTP_CODE do_request();                                 // 对http请求发出响应
    char *get_line() { return m_read_buf + m_start_line; }; // 获取一行数据
    LINE_STATUS parse_line();                               // 从状态机，用于分析出一行内容
    void unmap();                                           // 解除文件内存映射
    bool add_response(const char *format, ...);             // 添加响应
    bool add_content(const char *content);                  // 添加响应内容
    bool add_status_line(int status, const char *title);    // 添加响应状态行
    bool add_headers(int content_length);                   // 添加http响应报文的头部字段
    bool add_content_type();                                // 添加内容类型
    bool add_content_length(int content_length);            // 添加内容实体的长度
    bool add_linger();                                      // 添加响应报文的Connection字段，指定是否保持连接
    bool add_blank_line();                                  // 添加\r\n

public:
    static int m_epollfd;    // epoll文件句柄
    static int m_user_count; //

private:
    int m_sockfd;                        // 当前连接的client sockdfd
    sockaddr_in m_address;               //
    char m_read_buf[READ_BUFFER_SIZE];   // 读缓冲区
    int m_read_idx;                      // 标示读缓冲区已经读入的客户数据的最后一个字节的下一个位置
    int m_checked_idx;                   // 当前正在分析的字符在读缓冲区的位置
    int m_start_line;                    // 当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; //
    int m_write_idx;                     // 写缓冲区中待发送的字节数
                                         //
    CHECK_STATE m_check_state;           // 主状态机的状态
    METHOD m_method;                     // 请求的方法
                                         //
    char m_real_file[FILENAME_LEN];      // 客户请求文件的完整路径
    char *m_url;                         // 请求的URL，服务器资源目录
    char *m_version;                     // http版本信息，我们只支持1.1
    char *m_host;                        // 主机信息
    int m_content_length;                // http请求的消息体的长度
    bool m_linger;                       // http请求是否要保持链接
                                         //
    char *m_file_address;                // 客户请求的目标文件被映射到内存中位置
    struct stat m_file_stat;             // 目标文件的状态
    struct iovec m_iv[2];                //
    int m_iv_count;                      //
    int cgi;                             //是否启用的POST
    char *m_string;                      //存储请求头数据
};
#endif
