// 2022/4/28
// webserver实现文件

#include "webserver.h"

webserver::webserver(std::string dir_path)
{
    // http_conn类对象
    users = new http_conn[MAX_FD];

    // 网站根目录
    char server_path[200];
    getcwd(server_path, 200);
    m_root = (char *)malloc(strlen(server_path) + dir_path.size() + 1);
    strcpy(m_root, server_path);
    strcat(m_root, dir_path.c_str());

    //定时器
    users_timer = new client_data[MAX_FD];
}

webserver::~webserver()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_thread_pool;
}

void webserver::init(int listen_port,
                     std::string db_user, std::string db_passwd, std::string db_name,
                     int is_close_log, int log_write_mode,
                     int opt_linger,
                     int trig_mode,
                     int num_sql, int num_thread,
                     int actor_mode)
{
    m_listen_port = listen_port;
    m_db_user = db_user;
    m_db_passwd = db_passwd;
    m_db_name = db_name;
    m_is_close_log = is_close_log;
    m_log_write_mode = log_write_mode;
    m_opt_linger = opt_linger;
    m_trig_mode = trig_mode;
    m_num_sql = num_sql;
    m_num_thread = num_thread;
    m_actor_mode = actor_mode;
}

void webserver::thread_pool()
{
    m_thread_pool = new threadpool<http_conn>(m_actor_mode, m_conn_pool, m_num_thread);
}

void webserver::sql_pool()
{
    // 初始化数据库连接池
    m_conn_pool = connection_pool::get_instance();
    m_conn_pool->init("localhost", m_db_user, m_db_passwd, m_db_name, 3306, m_num_sql, m_is_close_log);

    // 初始化数据库读取表
    users->init_mysql_result(m_conn_pool);
}

void webserver::log_write()
{
    if (0 == m_is_close_log)
    {
        //初始化日志
        if (1 == m_log_write_mode)
        {
            Log::get_instance()->init("./logs/ServerLog", m_is_close_log, 2000, 800000, 800);
        }

        else
        {
            Log::get_instance()->init("./logs/ServerLog", m_is_close_log, 2000, 800000, 0);
        }
    }
}

void webserver::trig_mode()
{
    // LT + LT
    if (0 == m_trig_mode)
    {
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 0;
    }
    // LT + ET
    else if (1 == m_trig_mode)
    {
        m_listen_trig_mode = 0;
        m_conn_trig_mode = 1;
    }
    // ET + LT
    else if (2 == m_trig_mode)
    {
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 0;
    }
    // ET + ET
    else if (3 == m_trig_mode)
    {
        m_listen_trig_mode = 1;
        m_conn_trig_mode = 1;
    }
}

void webserver::event_listen()
{
    // 网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    if (0 == m_opt_linger)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_opt_linger)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    // 监听所有网卡
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_listen_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 初始化定时器
    util.init(TIMESLOT);

    // epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    // 绑定listenfd
    util.addfd(m_epollfd, m_listenfd, false, m_listen_trig_mode);
    http_conn::m_epollfd = m_epollfd;

    // 绑定管道fd
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    util.setnonblocking(m_pipefd[1]);
    util.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 添加信号处理函数
    util.addsig(SIGPIPE, SIG_IGN);
    util.addsig(SIGALRM, util.sig_handler, false);
    util.addsig(SIGTERM, util.sig_handler, false);

    // 设置定时器，事件到了会发出SIGALRM信号，然后发送给管道
    alarm(TIMESLOT);

    // 工具类,信号和描述符基础操作
    utils::u_pipefd = m_pipefd;
    utils::u_epollfd = m_epollfd;
}

void webserver::event_loop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = deal_clinet_data();
                if (false == flag)
                    continue;
            }
            // 服务器端关闭连接
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 定时器的处理信号事件
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_with_signal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                deal_with_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                deal_with_write(sockfd);
            }
        }
        // 如果有定时器超时，则调用超时处理函数处理超时的连接和重新设置定时器事件
        if (timeout)
        {
            util.timer_handler();
            // LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}

bool webserver::deal_clinet_data()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_listen_trig_mode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            util.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        // 创建用户连接的定时器
        timer(connfd, client_address);
    }
    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                util.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            // 创建用户连接的定时器
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

// 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
void webserver::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_conn_trig_mode, m_is_close_log, m_db_user, m_db_passwd, m_db_name);

    // 初始化client_data数据
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;

    // 将定时器添加到链表中
    util.m_timer_lst.add_timer(timer);
}

// 处理信号，有定时器超时信号和关闭程序信号
bool webserver::deal_with_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                // LOG_INFO("timer tick");
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void webserver::deal_with_read(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor
    if (1 == m_actor_mode)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        // 若监测到读事件，将该事件放入请求队列
        m_thread_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // proactor
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_thread_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void webserver::deal_with_write(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (1 == m_actor_mode)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_thread_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

// 若有数据传输，则将定时器往后延迟3个单位，并对新的定时器在链表上的位置进行调整
void webserver::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    util.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void webserver::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        util.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}