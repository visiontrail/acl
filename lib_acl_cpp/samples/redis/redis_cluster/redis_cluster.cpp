#include "stdafx.h"
#include "util.h"

static acl::string __keypre("test_key_cluster");

static bool test_del(acl::redis_key& option, int i)
{
	acl::string key;

	key.format("%s_%d", __keypre.c_str(), i);
	option.reset();
	int ret = option.del(key.c_str(), NULL);
	if (ret < 0)
	{
		printf("del key: %s error\r\n", key.c_str());
		return false;
	}
	else if (i < 10)
		printf("del ok, key: %s\r\n", key.c_str());
	return true;
}

static bool test_expire(acl::redis_key& option, int i)
{
	acl::string key;

	key.format("%s_%d", __keypre.c_str(), i);
	option.reset();
	if (option.expire(key.c_str(), 100) < 0)
	{
		printf("expire key: %s error\r\n", key.c_str());
		return false;
	}
	else if (i < 10)
		printf("expire ok, key: %s\r\n", key.c_str());
	return true;
}

static bool test_ttl(acl::redis_key& option, int i)
{
	acl::string key;
	int ttl;

	key.format("%s_%d", __keypre.c_str(), i);
	option.reset();
	if ((ttl = option.ttl(key.c_str())) < 0)
	{
		printf("get ttl key: %s error\r\n", key.c_str());
		return false;
	}
	else if (i < 10)
		printf("ttl ok, key: %s, ttl: %d\r\n", key.c_str(), ttl);
	return true;
}

static bool test_exists(acl::redis_key& option, int i)
{
	acl::string key;

	key.format("%s_%d", __keypre.c_str(), i);
	option.reset();
	if (option.exists(key.c_str()) == false)
	{
		if (i < 10)
			printf("no exists key: %s\r\n", key.c_str());
	}
	else
	{
		if (i < 10)
			printf("exists key: %s\r\n", key.c_str());
	}
	return true;
}

static bool test_type(acl::redis_key& option, int i)
{
	acl::string key;

	key.format("%s_%d", __keypre.c_str(), i);
	option.reset();
	acl::redis_key_t ret = option.type(key.c_str());
	if (ret == acl::REDIS_KEY_UNKNOWN)
	{
		printf("unknown type key: %s\r\n", key.c_str());
		return false;
	}
	else if (i < 10)
		printf("type ok, key: %s, ret: %d\r\n", key.c_str(), ret);
	return true;
}

static bool test_set(acl::redis_string& option, int i)
{
	acl::string key;
	key.format("%s_%d", __keypre.c_str(), i);

	acl::string value;
	value.format("value_%s", key.c_str());

	option.reset();
	bool ret = option.set(key.c_str(), value.c_str());
	return ret;
	if (i < 10)
		printf("set key: %s, value: %s %s\r\n", key.c_str(),
			value.c_str(), ret ? "ok" : "error");
	return ret;
}

static bool test_get(acl::redis_string& option, int i)
{
	acl::string key;
	key.format("%s_%d", __keypre.c_str(), i);

	acl::string value;

	option.reset();
	bool ret = option.get(key.c_str(), value);
	if (i < 10)
		printf("get key: %s, value: %s %s, len: %d\r\n",
			key.c_str(), value.c_str(), ret ? "ok" : "error",
			(int) value.length());
	return ret;
}

static int __threads_exit = 0;

class test_thread : public acl::thread
{
public:
	test_thread(acl::locker& locker, acl::redis_cluster& cluster,
		int max_conns, const char* cmd, int n)
	: locker_(locker)
	, cluster_(cluster)
	, max_conns_(max_conns)
	, cmd_(cmd)
	, n_(n)
	{}

	~test_thread() {}

protected:
	virtual void* run()
	{
		bool ret;
		acl::redis_key cmd_key;
		acl::redis_string cmd_string;

		for (int i = 0; i < n_; i++)
		{
			cmd_key.set_cluster(&cluster_, max_conns_);
			cmd_string.set_cluster(&cluster_, max_conns_);

			if (cmd_ == "set")
				ret = test_set(cmd_string, i);
			else if (cmd_ == "get")
				ret = test_get(cmd_string, i);
			else if (cmd_ == "del")
				ret = test_del(cmd_key, i);
			else if (cmd_ == "expire")
				ret = test_expire(cmd_key, i);
			else if (cmd_ == "ttl")
				ret = test_ttl(cmd_key, i);
			else if (cmd_ == "exists")
				ret = test_exists(cmd_key, i);
			else if (cmd_ == "type")
				ret = test_type(cmd_key, i);
			else if (cmd_ == "all")
			{
				if (test_set(cmd_string, i) == false
					|| test_get(cmd_string, i) == false
					|| test_exists(cmd_key, i) == false
					|| test_type(cmd_key, i) == false
					|| test_expire(cmd_key, i) == false
					|| test_ttl(cmd_key, i) == false
					|| test_del(cmd_key, i) == false)
				{
					ret = false;
				}
				else
					ret = true;
			}
			else
			{
				printf("unknown cmd: %s\r\n", cmd_.c_str());
				break;
			}

			if (ret == false)
			{
				printf("cmd: %s error, tid: %lu\r\n",
					cmd_.c_str(), thread_self());
				break;
			}

			continue;

			if (i > 0 && i % 1000 == 0)
			{
				char tmp[128];
				acl::safe_snprintf(tmp, sizeof(tmp), "%d", i);
				acl::meter_time(__FILE__, __LINE__, tmp);
			}
		}

		locker_.lock();
		__threads_exit++;
		locker_.unlock();

		return NULL;
	}

private:
	acl::locker& locker_;
	acl::redis_cluster& cluster_;
	int max_conns_;
	acl::string cmd_;
	int n_;
};

static void usage(const char* procname)
{
	printf("usage: %s -h[help]\r\n"
		"-s redis_addr_list[127.0.0.1:6379]\r\n"
		"-n count[default: 10]\r\n"
		"-C connect_timeout[default: 10]\r\n"
		"-I rw_timeout[default: 10]\r\n"
		"-c max_threads[default: 10]\r\n"
		"-a cmd[set|get|expire|ttl|exists|type|del]\r\n",
		procname);
}

int main(int argc, char* argv[])
{
	int  ch, n = 1, conn_timeout = 10, rw_timeout = 10;
	int  max_threads = 10;
	acl::string addrs("127.0.0.1:6379"), cmd;

	while ((ch = getopt(argc, argv, "hs:n:C:I:c:a:")) > 0)
	{
		switch (ch)
		{
		case 'h':
			usage(argv[0]);
			return 0;
		case 's':
			addrs = optarg;
			break;
		case 'n':
			n = atoi(optarg);
			break;
		case 'C':
			conn_timeout = atoi(optarg);
			break;
		case 'I':
			rw_timeout = atoi(optarg);
			break;
		case 'c':
			max_threads = atoi(optarg);
			break;
		case 'a':
			cmd = optarg;
			break;
		default:
			break;
		}
	}

	acl::acl_cpp_init();
	acl::log::stdout_open(true);

	acl::redis_cluster cluster(conn_timeout, rw_timeout);

	// 当连接池不可用时，设置重新恢复该连接池对象的等待时间(秒)，
	// 当设置的值 <= 0 时表示不再恢复
	cluster.set_retry_inter(0);

	// 设置重定向的最大阀值，若重定向次数超过此阀值则报错
	cluster.set_redirect_max(20);

	// 当重定向次数 >= 2 时每次再重定向此函数设置休息的时间(秒)
	cluster.set_redirect_sleep(1);

	cluster.init(NULL, addrs.c_str(), max_threads);

	struct timeval begin;
	gettimeofday(&begin, NULL);

	acl::locker locker;

	std::vector<test_thread*> threads;
	for (int i = 0; i < max_threads; i++)
	{
		test_thread* thread = new test_thread(locker, cluster,
			max_threads, cmd.c_str(), n);
		threads.push_back(thread);
		thread->set_detachable(true);
		thread->start();
	}

	while (true)
	{
		locker.lock();
		if (__threads_exit == max_threads)
		{
			locker.unlock();
			printf("All threads over now!\r\n");
			break;
		}
		locker.unlock();

		//printf("max_threads: %d, threads_exit: %d\r\n",
		//	max_threads, __threads_exit);
		sleep(1);
	}

	std::vector<test_thread*>::iterator it = threads.begin();
	for (; it != threads.end(); ++it)
	{
		//(*it)->wait();
		delete (*it);
	}

	struct timeval end;
	gettimeofday(&end, NULL);

	long long int total = max_threads * n;
	double inter = util::stamp_sub(&end, &begin);
	printf("total %s: %lld, spent: %0.2f ms, speed: %0.2f\r\n", cmd.c_str(),
		total, inter, (total * 1000) /(inter > 0 ? inter : 1));

#ifdef WIN32
	printf("enter any key to exit\r\n");
	getchar();
#endif
	return 0;
}