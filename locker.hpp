#pragma once
#ifndef LOCKER_HPP
#define LOCKER_HPP

#include <iostream>
#include <pthread.h>
#include <semaphore.h>

//�������࣬��װ��һ��pthread_mutex_t������
class Locker
{
public:
	//���캯��
	Locker()
	{
		//��ʼ��
		//�Զ�̬��ʽ�������������������֮��᷵��0���κ���������ֵ��˵����������������
		if (pthread_mutex_init(&m_mutex, NULL) != 0)
		{
			std::cout << "Locker: init mutex locker fail" << std::endl;
			//�׳��쳣
			throw std::exception();
		}
	}

	//��������
	~Locker()
	{
		//���ٻ�����
		pthread_mutex_destroy(&m_mutex);
	}

	//����
	bool lock()
	{
		return pthread_mutex_lock(&m_mutex);
	}

	//����
	bool unlock()
	{
		return pthread_mutex_unlock(&m_mutex);
	}

	//��ȡ�������
	pthread_mutex_t* get()
	{
		return &m_mutex;
	}

private:
	// ���廥��������
	pthread_mutex_t m_mutex;
};

// ������������
class Condition
{
public:
	//���캯��
	Condition()
	{
		//��pthread_mutex_init��ͬ�������������������Ĵ������򷵻�0
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

	//����������
	bool wait(pthread_mutex_t* t_mutex)
	{
		return pthread_cond_wait(&m_cond, t_mutex) == 0;
	}

	//��������������
	//pthread_cond_timedwait()���ڵȴ�һ�������������ȴ�����������ͬʱ�������õȴ���ʱ��
	//����һ���ǳ����õĹ��ܣ��������һֱ�ȴ�ĳһ�����������Ϳ���ʹ�����������
	bool timeWait(pthread_mutex_t* t_mutex, struct timespec t)
	{
		return pthread_cond_timedwait(&m_cond, t_mutex, &t) == 0;
	}

	//�����߳�
	//������������pthread_cond_wait(&cond1,&mutex1)������һ���̡߳�
	bool signal()
	{
		return pthread_cond_signal(&m_cond) == 0;
	}

	//�㲥���������߳�
	//������������pthread_cond_wait(&cond1,&mutex1)���߳�
	bool broadcast()
	{
		return pthread_cond_broadcast(&m_cond) == 0;
	}

private:
	pthread_cond_t m_cond;
};


//�ź�����
class Sem
{
public:
	Sem()
	{
		//����sem_init����������ʼ��һ���ź���
		//extern int sem_init __P ((sem_t *__sem, int __pshared, unsigned int __value));����
		//pshared��Ϊ��ʱ���ź����ڽ��̼乲������ֻ��Ϊ��ǰ���̵������̹߳���
		//value�������ź����ĳ�ʼֵ����
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
		//����sem_destroy(sem_t *sem)�����ͷ��ź���sem��
		sem_destroy(&m_sem);
	};

	//p����
	bool wait()
	{
		//����sem_wait( sem_t *sem )������������ǰ�߳�ֱ���ź���sem��ֵ����0�����������sem��ֵ��һ������������Դ��ʹ�ú���١�
		return sem_wait(&m_sem) == 0;
	}

	//v����
	bool post()
	{
		//����sem_post( sem_t *sem )���������ź�����ֵ
		return sem_post(&m_sem) == 0;
	}

private:
	// �ź���
	//  �ź�������������Ϊ�ṹsem_t
	sem_t m_sem;
};

#endif