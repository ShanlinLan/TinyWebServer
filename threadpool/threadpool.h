// 2022/4/28
// 线程池

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../log/log.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    threadpool(int actor_mode, connection_pool *conn_pool, int thread_number = 8, int max_request = 10000, int is_close_log = 0);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    // 工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;          // 线程池中的线程数
    int m_max_requests;           // 请求队列中允许的最大请求数
    pthread_t *m_threads;         // 描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue;   // 请求队列
    locker m_queuelocker;         // 保护请求队列的互斥锁
    sem m_queuestat;              // 是否有任务需要处理
    connection_pool *m_conn_pool; // 数据库连接池
    int m_actor_mode;             // 模型切换
    int m_is_close_log;           // 是否关闭日志
};

// 初始化线程池
template <typename T>
threadpool<T>::threadpool(int actor_mode, connection_pool *conn_pool, int thread_number, int max_requests, int is_close_log)
    : m_actor_mode(actor_mode), m_thread_number(thread_number), m_max_requests(max_requests),
      m_threads(NULL), m_conn_pool(conn_pool), m_is_close_log(is_close_log)
{
    if (thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
        LOG_INFO("Create %dth Thread.", i + 1);
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

// Reactor模式下往工作队列中添加任务
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    // 设置请求状态
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// proactor模式下往工作队列中添加任务，只会将读任务加入队列
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 线程池开始工作
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

// 线程池运行
template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        // 根据设置的工作模式进行不同处理
        if (1 == m_actor_mode) // recactor模式
        {
            if (0 == request->m_state)  // 读
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connection_RAII mysqlcon(&request->mysql, m_conn_pool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else    // 写
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else // proactor模式只有读
        {
            connection_RAII mysqlcon(&request->mysql, m_conn_pool);
            request->process();
        }
    }
}

#endif