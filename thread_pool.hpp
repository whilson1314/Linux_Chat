#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <pthread.h>
#include <iostream>
#include <list>
#include "locker.hpp"
#include <exception>

//线程池类
//模板参数T：任务类
template<typename T>
class ThreadPool
{
public:
    /*
    args:
        thread_sum      线程数量
        max_req         任务队列中最多允许的等待请求数量
    */
   ThreadPool(int thread_sum = 0, int max_req = 0);
   
   ~ThreadPool();

   //扩展任务
   bool append(T* task);

   //工作线程，内部调用run执行具体任务
   static void* worker(void* obj);

    //线程运行
   void run();

private:
    //线程数量
    int m_thread_num;

    //线程池数组头指针，大小为m_thread_num
    pthread_t* m_threads;

    //最大请求数
    int m_max_req;

    //线程池任务队列
    std::list<T*> m_work_queue;

    //队列互斥锁
    Locker m_locker_queue;

    //队列信号量
    Sem m_sem_queue;

    //队列条件变量
    //Condition m_cond_queue;

    //线程池停止标志
    bool m_is_stop;

};

//构造函数
template<typename T>
ThreadPool<T>::ThreadPool(int thread_pool, int max_que): m_thread_num(thread_num), m_max_req(max_req), m_is_stop(false), m_threads(NULL)
{
    //如果线程池参数不合法
    if(thread_num <= 0 || max_req <= 0)
    {
        std::cout << "ThreadPool : init threadpool fail : args invalid!" << std::endl;
        throw std::exception();
    }

    //根据传入的线程数量，创建线程标识符数组
    m_threads = new pthread_t[m_thread_num];
    if(!m_threads)
    {
        std::cout << "ThreadPool : init threadPool fail : no memory!" << std::endl;
        throw std::exception();
    }

    //创建线程池
    for(int i = 0; i < thread_num; i++)
    {
        std::cout << "ThreadPool : creating the " << i << "th thread!" << std::endl;

        //调用pthread_create 创建线程，回调函数worker，并把当前对象作为参数传入
        if(pthread_create(m_thread + i, NULL, worker, this) != 0)
        {
            std::cout << "ThreadPool: creating the" << i << "th thread fail" << std::endl;
            std::cout << "ThreadPool: delete all threads" << std::endl;
            delete[] m_threads;
            throw std::exception();
        }

        //将线程分离
        if(pthread_detach(m_threads[i] != 0))
        {
            std::cout << "ThreadPool: detach the" << i << "th thread fail!" << std::endl;
            std::cout << "ThreadPool: delete all threads" << std::endl;
            delete[] m_threads;
            throw std::exception();
        }
    }

    std::cout << "ThreadPool: create threadpool succeeded!" << std::endl;
}

//析构函数
template<typename T>
ThreadPool<T>::~ThreadPool()
{
    //回收所有线程
    delete[] m_threads;
    //停止运行线程池
    m_is_stop = true;
}

//添加任务队列
template<typename T>
bool ThreadPool<T>::append(T* req)
{
    //加锁，保证访问任务队列护持（TODO：返回bool值，暂未判断）
    m_locker_queue.lock();
    if(m_locker_queue.size() >= m_max_req)
    {
        std::cout << "ThreadPool: append fail : requests is full" << std::endl;
        m_locker_queue.unlock();
        return false;
    }
    //添加任务
    m_work_queue.push_back(req);
    //解锁
    m_locker_queue.unlock();
    m_sem_queue.post(); //唤醒线程
    return true;
}

//工作线程
template<typename T>
void* ThreadPool<T>:: worker(void* obj)
{
    ThreadPool* pool = (ThreadPool*) obj;
    pool->run();
    return pool;
}

//工作线程执行内容
template<typename T>
void ThreadPool<T>::run()
{
    //线程池还在运行
    while(!m_is_stop)
    {
        //尝试获取任务
        m_sem_queue.wait(); //创建后则阻塞在获取信号量等待

        //从队列中获取任务
        m_locker_queue.lock();
        if(m_work_queue.empty())
        {
            m_locker_queue.unlock();
            continue;
        }
        //获取任务 线程池队列m_work_queue 是一个std::list
        T* req = m_work_queue.front();
        m_work_queue.pop_front();
        m_work_queue.unlock();

        //任务为空，再次循环获取
        if(!req)
        {
            continue;
        }

        //任务不为空，处理任务
        req->process();

    }

}

#endif