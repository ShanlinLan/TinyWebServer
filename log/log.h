// 2022/4/26
// 日志头文件

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "block_queue.h"

class Log
{
public:
    // C++11以后，使用局部变量懒汉不用加锁
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    // 异步日志线程工作函数
    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
        return NULL;
    }

    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    // 同步写入日志
    void write_log(int level, const char *format, ...);

    // 同步刷新缓冲流
    void flush(void);

private:
    Log();
    virtual ~Log();

    // 从阻塞队列中取出一个日志string，写入文件
    void *async_write_log()
    {
        std::string single_log;
        while (m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
        return NULL;
    }

    char dir_name[128];                    // 路径名
    char log_name[128];                    // log文件名
    int m_split_lines;                     // 日志最大行数
    int m_log_buf_size;                    // 日志缓冲区大小
    long long m_count;                     // 日志行数记录
    int m_today;                           // 记录当前时间是哪一天
    FILE *m_fp;                            // 打开log文件的文件指针
    char *m_buf;                           // 日志缓冲区指针
    block_queue<std::string> *m_log_queue; // 阻塞日志队列
    bool m_is_async;                       // 是否同步标志位
    locker m_mutex;
    int m_is_close_log; // 关闭日志
};

#define LOG_DEBUG(format, ...)                                    \
    if (0 == m_is_close_log)                                      \
    {                                                             \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_INFO(format, ...)                                     \
    if (0 == m_is_close_log)                                      \
    {                                                             \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_WARN(format, ...)                                     \
    if (0 == m_is_close_log)                                      \
    {                                                             \
        Log::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_ERROR(format, ...)                                    \
    if (0 == m_is_close_log)                                      \
    {                                                             \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

#endif