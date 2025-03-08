#include "threadpool.h"
#include <thread>
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
// 初始化任务队列的上限阈值和线程数量的阈值
const int THREAD_MAX_IDLE_TIME = 5; //单位：秒
////////////////////////////////////////////ThreadPool方法实现
// 线程池的构造函数
ThreadPool::ThreadPool()
	:initThreadSize_(4)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(ThreadPoolMode::MODE_FIXED)
	, isPoolRuning(false)
	, curThreadSize_(0)
	, idleThreadSize_(0)
	, threaSizeThreshHold(THREAD_MAX_THRESHHOLD)
{
}
// 线程池的析构
ThreadPool::~ThreadPool() {
	// 因为构造的时候我们没有new东西 所以就没什么东西就行析构
	isPoolRuning = false;
	//等待线程池里面所有的线程返回 
}

//设置线程池的工作模式
void ThreadPool::setTheadPoolMode(ThreadPoolMode Mode) {
	if (checkRunningState()) {
		return;
	}
	this->poolMode_ = Mode;
}


//设置task任务队列上限阈值 
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {
	this->taskQueMaxThreshHold_ = threshhold;
}

//给线程池提交任务  用户调用该接口 传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	// 因为是用户和线程列表一起对这个任务对象操作的 就是用户是生产者 而线程列表是消费者
	/// 两者同时操作的话 肯定是不合理的 所以要进行线程同步

	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx);
	// 线程的通信 等待任务队列有空余 如果有空余 把任务放入到任务队列中
	// 因为新放入了任务 任务队列肯定不空了 在notEmpty上进行通知
	// wait:一直等 等到条件满足后才运行当前线程
	// wait_for: 最多等多长事件 如果超过就不等了 返回值为BOOL
	// wait_until: 等你到下个星期一 持续等待的事件 设置一个时间终止节点
	/*while (taskQue_.size() == taskQueMaxThreshHold_) {
		notFull_.wait(lock);
	}*/
	// 这行代码和上面的while循环是等价的
	// 用户提交任务 ，最长不能阻塞超过1s，否则判断提交任务失败，返回，这就是服务的降级 告诉用户后台没有空余的队列
	// 这里的意思 就是如果用户提交任务而线程池本身的原因导致任务队列一直不为空 导致用户阻塞在这里
	// 会给用户不好的体验 可以加以判断如果一秒内没有提交成功 则返回提交失败 
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool
		{return taskQue_.size() < static_cast<size_t>(taskQueMaxThreshHold_); })) {
		std::cerr << "task queue is full,submit task fail." << std::endl;
		// wait_for返回值如果为false的话 就说明等待一秒条件依旧没有满足
		return Result(sp,false);
	}
	// 如果任务队列是满的话 就在这里阻塞 释放前面的锁 并且改变当前线程状态 运行->阻塞
// 如果有空余，把任务放入到任务队列中
	taskQue_.emplace(sp);
	taskSize_++; 

	// 因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知 通知消费者（线程）
	notEmpty_.notify_all();

	// cached模式 任务处理比较紧急 场景： 小而快的任务 需要根据任务数量和空闲线程的数量 判断是否需要创建新的线程
	if (poolMode_ == ThreadPoolMode::MODE_CACHED 
		&& taskSize_>idleThreadSize_ 
		&& curThreadSize_< threaSizeThreshHold) {
		// 创建新线程
		std::cout << ">>> create new thread" << std::this_thread::get_id() << " exit!" << std::endl;
		// 创建thread线程对象的时候 把线程函数给到thred线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		// make_unique是c++14里面的
		int threadId = ptr->getThreadId();
		threads_.emplace(threadId, std::move(ptr));
		// 启动线程
		threads_[threadId]->start();
	}
	// 返回任务的Result对象
	return Result(sp,true);
}

//开启线程池
void ThreadPool::start(int initThreadSize) {
	// 记录初始化线程数量
	this->initThreadSize_ = initThreadSize;
	this->curThreadSize_ = initThreadSize;
	// 设置线程池的运行状态
	this->isPoolRuning = true;
	//创建线程对象 先把线程加入到线程列表中
	for (int i = 0; i < this->initThreadSize_; i++) {
		// 创建thread线程对象的时候 把线程函数给到thred线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		// make_unique是c++14里面的
		int threadId = ptr->getThreadId();
		threads_.emplace(threadId,std::move(ptr));
		// 因为unique不允许拷贝构造 因为在容器中会调用对象的拷贝构造 所以我们需要转移这个对象的使用权
		// 从实参转移到形参
	}
	// 启动所有线程	std::vector<Thread*> threads_;  //线程列表(线程队列) 
	for (int i = 0; i < initThreadSize_; i++) {
		// 单独在线程对象中执行每个对象要执行的线程函数
		threads_[i]->start(); // 每个线程需要去执行一个线程函数的 
		idleThreadSize_++; //记录空闲线程的数量
	}
}


// 启动线程执行函数   线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int thread) { //线程函数返回，相应的线程也就结束了
	/*std::cout << "begin threadFunc  tid : "
		<< std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc  tid : "
		<< std::this_thread::get_id() << std::endl;*/
	auto lastime = std::chrono::high_resolution_clock().now();
	for (;;) {
		std::shared_ptr<Task> task;
		// 先获取锁
		{
			std::unique_lock<std::mutex> lock(taskQueMtx);
			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务..." << std::endl;
			// cashed模式下 有可能已经创建了很多的线程但是空闲时间超过60s，应该把多余的线程结束回收掉
			// （超过initThreadSize_数量的线程进行回收）
			// 当前时间-上一次线程执行的时间>60s
			if (poolMode_ == ThreadPoolMode::MODE_CACHED) {
				// 每一秒中返回一次  怎么区分：超时返回？还是任务执行返回
				while (taskQue_.size() == 0) {
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_>initThreadSize_) {
							// 开始回收线程
							// 记录线程数量的相关变量的值修改
							// 把线程对象从线程列表容器中删除 没有办法匹配threadFuns对应那个thread对象
							// 找到threadid对应的thread对象 进而将其删除
							threads_.erase(_threadid); 
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "threadid " << std::this_thread::get_id() << " exit" << std::endl;
							return;
						}
					}
				}
			}
			// 等待notEmpty条件
			notEmpty_.wait(lock, [&]()->bool {return static_cast<bool>(taskQue_.size() > 0); });
			// 从任务队列中取出一个任务出来
			idleThreadSize_--;
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功..." << std::endl;
			// 如果依然有剩余任务 继续通知其他得线程执行任务 我是第一个吃螃蟹的 里面还有螃蟹 通知其他人也吃
			if (taskQue_.size() > 0) {
				notEmpty_.notify_all();
			}
			// 顺便也通知一下提交任务哪里 当前线程池不满 可以提交任务
			notFull_.notify_all();
		} // 因为我们不能当前线程获取到锁之后 就一直等这个线程运行完之后 才释放这把锁
		 // 这样会导致任何时候 只有一个线程在执行
		// 当前线程负责执行这个任务
		if (task != nullptr) {
			// 判断一下 防止出现异常情况 导致程序崩溃
			task->exec(); // 执行任务 并把任务的返回值setVal方法给到Result
			lastime = std::chrono::high_resolution_clock().now(); //更新线程执行完任务的时间
		}
		idleThreadSize_++;
	}
}

// 设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshhold) {
	if (checkRunningState()) {
		return;
	}
	if (poolMode_ == ThreadPoolMode::MODE_CACHED) {
		this->threaSizeThreshHold = threshhold;
	}
}
bool ThreadPool::checkRunningState() const {
	return isPoolRuning;
}

/////////////////////////////////////////////////////////////////////////////////Thread方法实现
// 线程构造
Thread::Thread(ThreadFunc func)
	:Func_(func)
	,threadId_(generateId_++) 
{

}
int Thread::generateId_ = 0;
// 线程析构
Thread::~Thread() {

}

// 真正的启动线程函数
void Thread::start() {
	// 创建一个线程来执行一个线程函数
	std::thread t(Func_,threadId_); //c++11来说 分为线程对象t 和线程函数Func_
	t.detach(); //设置分离线程
	//为什么要设置分离线程？
	// 因为如果不分离的话 线程对象t和执行线程函数Func_的线程是具有绑定关系的
	// 当这个start函数执行完后 线程对象t离开作用域 线程对象t会被析构 也就是执行线程函数的这个线程也终止了
	// 所以为了使线程对象不影响真正执行的线程 所以设置线程分离 各玩各的
}

int Thread::getThreadId() const {
	return threadId_;
}
///////////////////////////////////////////////////////////////////////////////// Semaphore方法实现
//获取一个信号量资源
void  Semaphore::wait() {
	std::unique_lock<std::mutex> lock(mtx_);
	// 等待信号量有资源，没有资源的话，会阻塞当前线程
	cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
	resLimit_--;
}

// 增加一个信号量资源
void Semaphore::post() {
	std::unique_lock<std::mutex> lock(mtx_);
	resLimit_++;
	cond_.notify_all();
}


///////////////////////////////////////////////////////////////////////////////////Result方法实现
Result::Result(std::shared_ptr<Task> task,bool isValid):isValid_(isValid),task_(task){
	task_->setResult(this); //这里面调用初始化Task对象
}

//获取返回值结果
Any Result::get(){
	if (!isValid_) {
		return " ";
	}
	sem_.wait(); //task任务如果没有执行完，这里会阻塞用户的线程
	return std::move(any_);
}

void Result::setVal(Any any) {
	// 存储task的返回值
	this->any_ = std::move(any);
	sem_.post(); // 已经获取到的任务的返回值
}


///////////////////////////////////////////////////////////////////////////////////Task方法实现

Task::Task():result_(nullptr) {

}

void Task::exec() {
	if (result_ != nullptr) {
		result_->setVal(run()); //进行多态调用
	}
}

void Task::setResult(Result* res) {
	this->result_ = res;
}