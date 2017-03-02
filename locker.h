#ifndef LOCKER_H_
#define LOCKER_H_

#include <stdexcept>
#include <pthread.h>
namespace mj{
	class locker
	{
	public:
		locker()
		{
			if (pthread_mutex_init(&m_mutex, NULL) != 0)
			{
				throw std::logic_error("locker: pthread_mutex_init failed");
			}
		}
		~locker()
		{
			pthread_mutex_destroy(&m_mutex);
		}
		/*no capy function*/
		locker(const locker &) = delete;
		locker &operator=(const locker &) = delete;
		
		bool lock()
		{
			return pthread_mutex_lock(&m_mutex) == 0;
		}
		bool unlock()
		{
			return pthread_mutex_unlock(&m_mutex) == 0;
		}

	private:
		pthread_mutex_t m_mutex;
	};

	class cond
	{
	public:
		cond() :condition(false)
		{
			if (pthread_mutex_init(&m_mutex, NULL) != 0)
			{
				throw std::logic_error("cond: pthread_mutex_init failed");
			}
			if (pthread_cond_init(&m_cond, NULL) != 0)
			{
				pthread_mutex_destroy(&m_mutex);
				throw std::logic_error("cond: pthread_cond_init failed");
			}
		}
		~cond()
		{
			pthread_mutex_destroy(&m_mutex);
			pthread_cond_destroy(&m_cond);
		}
		bool wait()
		{
			int ret = 0;
			pthread_mutex_lock(&m_mutex);
			ret = pthread_cond_wait(&m_cond, &m_mutex);
			pthread_mutex_unlock(&m_mutex);
			return ret == 0;
		}
		bool signal()
		{
			return pthread_cond_signal(&m_cond) == 0;
		}

	private:
		bool condition;
		pthread_mutex_t m_mutex;
		pthread_cond_t m_cond;
	};
}
#endif
