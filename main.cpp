#include "threadpool.h"
#include <chrono>
#include <thread>
/*
	有些场景，我们希望能够获取线程执行任务的返回值
	举例：1+2+.....+30000的和
	thread1: 1+2+.....+10000
	thread1: 10001+10002+.....+20000
	thread1: 20001+20002+.....+20000
	main thread: 给每一个线程分配计算的区间，并等待他们运算完后返回结果，合并最终的结果即可
*/


using Ulong = unsigned long long;
class MyTask :public Task 
{
public:
	MyTask(int begin, int end) :
		begin_(begin), end_(end)
	{}
	// 怎么设计run函数的返回值，可以表达任意的类型
	// 考虑使用模板作为函数的返回值 由于模板是编译期特性，
	// 模板参数的具体类型必须在编译时已知。然而，虚函数的调用取决于对象的实际类型，这只能在运行时确定。
	// c++17中有这个类型 就是Any类型
	Any run() { // run方法最终就在线程池分配的线程中去做事情了！
		std::cout << "tid: " << std::this_thread::get_id() << "begin!" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		Ulong sum = 0;
		for (Ulong i = begin_; i <= end_; i++) {
			sum += i;
		}
		std::cout << "tid: " << std::this_thread::get_id() << "end!" << std::endl;
		return sum;
	}
private:
	int begin_;
	int end_;
};
int main() {
	{
		ThreadPool pool;
		// 问题：ThreadPool对象析构以后，怎么样把线程池相关的资源全部回收？

		// 用户自己设置线程池的工作模式
		pool.setTheadPoolMode(ThreadPoolMode::MODE_CACHED);
		pool.start(4);
		// 如何设计这里的Result机制
		// 线程通信：submitTask 线程 执行完之后需要返回结果就是
		//Result res = pool.submitTask(std::make_shared<MyTask>());
		// get返回的是一个Any 类型 那么如何变成我们想要的类型呢？
		//res.get.cast_();
		//Any any = pool.submitTask(std::make_shared<MyTask>(1,4));

		// 随着task被执行完，task对象没了 依赖于task对象的Result对象也没了

		// 执行1到3亿的和 三个线程分别是1到一亿 一亿到两亿 两亿到三亿
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Ulong sum1 = res1.get().cast_<Ulong>();
		Ulong sum2 = res2.get().cast_<Ulong>();
		Ulong sum3 = res3.get().cast_<Ulong>();
		// Master - Slave线程模型
		// Master线程用来分解任务，然后给各个Salve线程分配任务
		// 等待各个Slave线程执行完任务，返回结果
		// Master
		std::cout << (sum1 + sum2 + sum3) << std::endl;
	}
	int ret = getchar();
	return 0;
}