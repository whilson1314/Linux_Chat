#pragma once
#ifndef LOCKER_HPP
#define LOCKER_HPP

#include <iostream>
#include <pthread.h>
#include <semaphore.h>

//互斥锁类，封装了一个pthread_mutex_t互斥量
class Locker
{
public:
	//构造函数
	Locker()
	{
		//初始化
		//以动态方式创建互斥锁，函数完成之后会返回0，任何其他返回值都说明互斥锁创建错误
		if (pthread_mutex_init(&m_mutex, NULL) != 0)
		{
			std::cout << "Locker: init mutex locker fail" << std::endl;
			//抛出异常
			throw std::exception();
		}
	}

	//析构函数
	~Locker()
	{
		//销毁互斥锁
		pthread_mutex_destroy(&m_mutex);
	}

	//加锁
	bool lock()
	{
		return pthread_mutex_lock(&m_mutex);
	}

	//解锁
	bool unlock()
	{
		return pthread_mutex_unlock(&m_mutex);
	}

	//获取互斥变量
	pthread_mutex_t* get()
	{
		return &m_mutex;
	}

private:
	// 定义互斥锁变量
	pthread_mutex_t m_mutex;
};

// 条件变量锁类
class Condition
{
public:
	//构造函数
	Condition()
	{
		//和pthread_mutex_init相同，如果完成条件变量锁的创建，则返回0
		if (pthread_cond_init(&m_cond, NULL) != 0)
		{
			std::cout << "Condition: init condition locker fail" << std::endl;
			throw std::exception();
		}
	}

	~Condition()
	{
		pthread_cond_destroy(&m_cond);
	}

	//阻塞条件锁
	bool wait(pthread_mutex_t* t_mutex)
	{
		return pthread_cond_wait(&m_cond, t_mutex) == 0;
	}

	//阻塞超期条件锁
	//pthread_cond_timedwait()用于等待一个条件变量，等待条件变量的同时可以设置等待超时。
	//这是一个非常有用的功能，如果不想一直等待某一条件变量，就可以使用这个函数。
	bool timeWait(pthread_mutex_t* t_mutex, struct timespec t)
	{
		return pthread_cond_timedwait(&m_cond, t_mutex, &t) == 0;
	}

	//唤醒线程
	//唤醒所有正在pthread_cond_wait(&cond1,&mutex1)的至少一个线程。
	bool signal()
	{
		return pthread_cond_signal(&m_cond) == 0;
	}

	//广播唤醒所有线程
	//唤醒所有正在pthread_cond_wait(&cond1,&mutex1)的线程
	bool broadcast()
	{
		return pthread_cond_broadcast(&m_cond) == 0;
	}

private:
	pthread_cond_t m_cond;
};


//信号量类
class Sem
{
public:
	Sem()
	{
		//函数sem_init（）用来初始化一个信号量
		//extern int sem_init __P ((sem_t *__sem, int __pshared, unsigned int __value));　　
		//pshared不为０时此信号量在进程间共享，否则只能为当前进程的所有线程共享；
		//value给出了信号量的初始值。　
		if (sem_init(&m_sem, 0, 0) != 0)
		{
			std::cout << "Sem:init semaphore fail" << std::endl;
			throw std::exception();
		}
	}

	Sem(int num)
	{
		if (sem_init(&m_sem, 0, num) != 0)
		{
			std::cout << "Sem:init semaphore with number fail" << std::endl;
			throw std::exception();
		}
	}

	~Sem()
	{
		//函数sem_destroy(sem_t *sem)用来释放信号量sem。
		sem_destroy(&m_sem);
	};

	//p操作
	bool wait()
	{
		//函数sem_wait( sem_t *sem )被用来阻塞当前线程直到信号量sem的值大于0，解除阻塞后将sem的值减一，表明公共资源经使用后减少。
		return sem_wait(&m_sem) == 0;
	}

	//v操作
	bool post()
	{
		//函数sem_post( sem_t *sem )用来增加信号量的值
		return sem_post(&m_sem) == 0;
	}

private:
	// 信号量
	//  信号量的数据类型为结构sem_t
	sem_t m_sem;
};

#endif