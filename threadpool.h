#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <list>
#include <cstdio>
#include <stdexcept>
#include <pthread.h>
#include <vector>
#include "locker.h"
namespace mj{
template< typename T >
class threadpool
{
	public:
		threadpool(size_t thread_num = 8, size_t max_req = 10000);
		~threadpool();
		bool append(T * request);//add a request to the workqueue

	private:
		static void* worker(void* arg);
		void run();

	private:
		const size_t MAX_THREAD_NUM = 20;
		const size_t MAX_REQUEST = 10000;
		size_t thread_number;
		size_t max_requests;
		std::vector<pthread_t> all_threads;
		std::list<T *> workqueue;
		locker queuelocker;
		cond request_come;
		static bool stop_all_thread;//
	};

	template< typename T >
	threadpool<T>::threadpool(size_t thread_num, size_t max_req) :
	thread_number(thread_num), max_requests(max_req), all_threads(thread_num, 0)
	{
		if ((thread_number > MAX_THREAD_NUM) || (max_requests > MAX_REQUEST))
			throw std::logic_error("threadpool: too many thread or request");

		for (int i = 0; i < thread_number; ++i)
		{
			if (pthread_create(&all_threads[i], NULL, worker, this) != 0)
				throw std::logic_error("threadpool: pthread_create failed");
			printf("create the %dth thread: %lu\n", i, all_threads[i]);
			if (pthread_detach(all_threads[i]))
				throw std::logic_error("threadpool: pthread_detach failed");
		}
	}

	template< typename T >
	threadpool< T >::~threadpool()
	{
		stop_all_thread = true;
	}

	template< typename T >
	bool threadpool< T >::append(T * request)
	{
		queuelocker.lock();
		if (workqueue.size() > max_requests)
		{
			queuelocker.unlock();
			return false;
		}
		workqueue.push_back(request);
		queuelocker.unlock();
		if(!request_come.signal())
			throw std::logic_error("threadpool: append->request_come.signal failed");
		return true;
	}

	template< typename T >
	void* threadpool< T >::worker(void* arg)
	{
		threadpool* pool = static_cast<threadpool*>(arg);
		pool->run();
		return static_cast<void *>(pool);
	}

	template< typename T >
	void threadpool< T >::run()
	{
		while (!stop_all_thread)
		{		
			request_come.wait();
			queuelocker.lock();
			if (workqueue.empty())
			{
				queuelocker.unlock();
				continue;
			}
			auto request = workqueue.front();
			workqueue.pop_front();
			queuelocker.unlock();
			if(!request)
				continue;
			request->process();
		}
	}
	template< typename T >
	bool threadpool<T>::stop_all_thread = false;
}

#endif
