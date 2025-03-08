#include "threadpool.h"
#include <chrono>
#include <thread>
/*
	��Щ����������ϣ���ܹ���ȡ�߳�ִ������ķ���ֵ
	������1+2+.....+30000�ĺ�
	thread1: 1+2+.....+10000
	thread1: 10001+10002+.....+20000
	thread1: 20001+20002+.....+20000
	main thread: ��ÿһ���̷߳����������䣬���ȴ�����������󷵻ؽ�����ϲ����յĽ������
*/


using Ulong = unsigned long long;
class MyTask :public Task 
{
public:
	MyTask(int begin, int end) :
		begin_(begin), end_(end)
	{}
	// ��ô���run�����ķ���ֵ�����Ա�����������
	// ����ʹ��ģ����Ϊ�����ķ���ֵ ����ģ���Ǳ��������ԣ�
	// ģ������ľ������ͱ����ڱ���ʱ��֪��Ȼ�����麯���ĵ���ȡ���ڶ����ʵ�����ͣ���ֻ��������ʱȷ����
	// c++17����������� ����Any����
	Any run() { // run�������վ����̳߳ط�����߳���ȥ�������ˣ�
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
		// ���⣺ThreadPool���������Ժ���ô�����̳߳���ص���Դȫ�����գ�

		// �û��Լ������̳߳صĹ���ģʽ
		pool.setTheadPoolMode(ThreadPoolMode::MODE_CACHED);
		pool.start(4);
		// �����������Result����
		// �߳�ͨ�ţ�submitTask �߳� ִ����֮����Ҫ���ؽ������
		//Result res = pool.submitTask(std::make_shared<MyTask>());
		// get���ص���һ��Any ���� ��ô��α��������Ҫ�������أ�
		//res.get.cast_();
		//Any any = pool.submitTask(std::make_shared<MyTask>(1,4));

		// ����task��ִ���꣬task����û�� ������task�����Result����Ҳû��

		// ִ��1��3�ڵĺ� �����̷ֱ߳���1��һ�� һ�ڵ����� ���ڵ�����
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		Ulong sum1 = res1.get().cast_<Ulong>();
		Ulong sum2 = res2.get().cast_<Ulong>();
		Ulong sum3 = res3.get().cast_<Ulong>();
		// Master - Slave�߳�ģ��
		// Master�߳������ֽ�����Ȼ�������Salve�̷߳�������
		// �ȴ�����Slave�߳�ִ�������񣬷��ؽ��
		// Master
		std::cout << (sum1 + sum2 + sum3) << std::endl;
	}
	int ret = getchar();
	return 0;
}