#ifndef  THREADPOOL_H
#define  THREADPOOL_H
#include <iostream>
#include <vector>
#include  <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
//example:
// ThreadPool pool;
// pool.start(4);
// class MyTask: public Task
//{
//public:
//	void run() {
//		// 线程代码
//	}
//}
// pool.submitTask(std::make_shared<MyTask>());

//	线程池支持的模式
enum class ThreadPoolMode {
	MODE_FIXED, //	固定数量的线程池模式
	MODE_CACHED, //	线程数量可动态增长的线程池模式
};

// 怎么构建一个Any类型？ 能让一个类型指向其他任意的类型 ？ 基类类型->派生类
// 该类中有个基类指针 指向一个派生类 然后派生类中有相应的返回值数据
// Any类型: 可以接受任意数据的类型
class Any {
	// 为什么模板的代码定义和声明需要在头文件当中？
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<class T>
	// 这个构造函数可以让Any类型接受其他任意其他的数据
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}
	template<class T>
	T cast_() {
		// 因为Any是父类对象 那么我们如何从父类找到他所指向的子类对象，取出它里面的data成员变量
		// 基类指针->派生类对象  RTTI 类型识别
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			std::cout << "pd是nullptr" << std::endl;
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	// 基类类型
	class Base {
	public:
		virtual ~Base() = default;
	};
	// 派生类类型
	template<class T>
	class Derive :public Base {
	public:
		Derive(T data) :data_(data) {}
		T data_; // 保存了任意的其他类型
	};
private:
	std::unique_ptr<Base> base_;
};


// 实现一个信号量类
class Semaphore {
public:
	Semaphore(){}
	Semaphore(int limit) :resLimit_(limit) {}
	~Semaphore() = default;

	//获取一个信号量资源
	void wait();

	// 增加一个信号量资源
	void post();
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};


// Task类型的前置声明
class Task;
// 实现提交到线程池的task任务执行完后的返回值类型Result
class Result 
{
public:
	~Result() = default;
	Result(std::shared_ptr<Task> task, bool isValid = true);
		// 问题1: setval方法 怎么获取任务执行完后的返回值

		// 问题2：get方法，用户调用这个方法获取task的返回值

	Any get();
	void setVal(Any any);
private:
	Any any_; // 存储任务的返回值

	Semaphore sem_; //线程通信信号量
	std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象
	std::atomic_bool isValid_; //返回值是否有效
};



//	线程类型
class Thread {
public:
	// 线程函数对象类型
	using ThreadFunc = std::function<void(int)>;

	// 线程构造
	Thread(ThreadFunc func);

	// 线程析构
	~Thread();

	// 真正的启动线程函数
	void start();

	// 获取线程id
	int getThreadId() const;
private:
	ThreadFunc Func_;
	static int generateId_;
	int threadId_; // 保存线程id
};


//	任务抽象基类
class Task {
	//	用户可以自定义任意的任务类型，从Task继承 重写run方法，实现自定义任务处理
public:
	Task();
	~Task() = default;
	virtual Any run() = 0;
	void exec();
	void setResult(Result* res);
private:
	Result *result_; // 不能使用Result的智能指针 会导致循环引用 而且Result对象也比Task对象时间长
};

//	线程池类型
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	//设置线程池的工作模式
	void setTheadPoolMode(ThreadPoolMode Mode);


	//设置task任务队列上限阈值 
	void setTaskQueMaxThreshHold(int threshhold);

	//给线程池提交任务 因为任务队列存放的就是智能指针对象 所以参数也是智能指针对象
	Result submitTask(std::shared_ptr<Task> sp);

	//开启线程池
	void start(int initThreadSize = 4);

	// 设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshhold);

	// 禁止线程池的拷贝赋值 消耗太大 没有必要对其复制
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程要执行的函数
	void threadFunc(int thread);

	// 检查pool的运行状态
	bool checkRunningState() const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_; //线程列表(线程队列) 
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	// 对于fixed模式不需要考虑线程安全模式 因为线程数量刚开始就是固定的 
	// 对于cached模式来说 存放线程的这个vector会有变化 要考虑线程安全
	// 利用智能指针 避免资源泄露 因为如果是裸指针的话 指针相对应的没有析构函数 所以会导致指针指向的那一片区域未释放

	size_t initThreadSize_; //初始的线程数量
	std::atomic_uint idleThreadSize_; // 记录空闲线程的数量
	std::atomic_uint curThreadSize_; //记录当前线程池里面线程的总数量
	int threaSizeThreshHold; //线程数量上限阈值
	std::queue<std::shared_ptr<Task>> taskQue_;//任务队列
	// 为什么存放的是任务抽象类的智能指针？
	//用户很有可能创建一个临时的任务对象进来 而我们用户在提交了这个任务之后 可能这个对象就被释放了
	//而这个任务对象可能还在我的任务队列中，这时候如果我们在调用这个任务对象进行处理，就会出现错误，
	// 所以我需要能够自动释放对象的资源

	std::atomic_uint taskSize_; //任务队列中任务的数量
	// 为什么使用atomic_uint？
	// 因为我们需要记录任务的个数 线程池消费一个任务 该数量减1 用户提交一个任务 该数量加1
	// 所以在不同线程中对于一个变量进行加加减减不是线程安全的,而我们又没必要对每次进行加加减减
	// 进行互斥锁进行互斥 这样会导致效率很慢 所以我们可以直接使用atomic_uint
	
	// 什么是atomic_uint？

	int taskQueMaxThreshHold_; //任务队列数量上限阈值 任务队列肯定不可能非常大 用户可以设置任务队列最大值

	std::mutex taskQueMtx; //保证任务队列的线程安全

	std::condition_variable notFull_; //表示任务队列不满
	std::condition_variable notEmpty_; //表示任务队列不空
	// 为什么定义两个条件变量？
	std::condition_variable exitCond_; // 等待线程资源全部回收
	ThreadPoolMode poolMode_; //当前线程池的工作模式

	std::atomic_bool isPoolRuning;		 // 表示当前线程池的启动状态
};

#endif

