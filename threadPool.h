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
//		// �̴߳���
//	}
//}
// pool.submitTask(std::make_shared<MyTask>());

//	�̳߳�֧�ֵ�ģʽ
enum class ThreadPoolMode {
	MODE_FIXED, //	�̶��������̳߳�ģʽ
	MODE_CACHED, //	�߳������ɶ�̬�������̳߳�ģʽ
};

// ��ô����һ��Any���ͣ� ����һ������ָ��������������� �� ��������->������
// �������и�����ָ�� ָ��һ�������� Ȼ��������������Ӧ�ķ���ֵ����
// Any����: ���Խ����������ݵ�����
class Any {
	// Ϊʲôģ��Ĵ��붨���������Ҫ��ͷ�ļ����У�
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<class T>
	// ������캯��������Any���ͽ���������������������
	Any(T data) :base_(std::make_unique<Derive<T>>(data)) {}
	template<class T>
	T cast_() {
		// ��ΪAny�Ǹ������ ��ô������δӸ����ҵ�����ָ����������ȡ���������data��Ա����
		// ����ָ��->���������  RTTI ����ʶ��
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			std::cout << "pd��nullptr" << std::endl;
			throw "type is unmatch!";
		}
		return pd->data_;
	}
private:
	// ��������
	class Base {
	public:
		virtual ~Base() = default;
	};
	// ����������
	template<class T>
	class Derive :public Base {
	public:
		Derive(T data) :data_(data) {}
		T data_; // �������������������
	};
private:
	std::unique_ptr<Base> base_;
};


// ʵ��һ���ź�����
class Semaphore {
public:
	Semaphore(){}
	Semaphore(int limit) :resLimit_(limit) {}
	~Semaphore() = default;

	//��ȡһ���ź�����Դ
	void wait();

	// ����һ���ź�����Դ
	void post();
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};


// Task���͵�ǰ������
class Task;
// ʵ���ύ���̳߳ص�task����ִ�����ķ���ֵ����Result
class Result 
{
public:
	~Result() = default;
	Result(std::shared_ptr<Task> task, bool isValid = true);
		// ����1: setval���� ��ô��ȡ����ִ�����ķ���ֵ

		// ����2��get�������û��������������ȡtask�ķ���ֵ

	Any get();
	void setVal(Any any);
private:
	Any any_; // �洢����ķ���ֵ

	Semaphore sem_; //�߳�ͨ���ź���
	std::shared_ptr<Task> task_; //ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_; //����ֵ�Ƿ���Ч
};



//	�߳�����
class Thread {
public:
	// �̺߳�����������
	using ThreadFunc = std::function<void(int)>;

	// �̹߳���
	Thread(ThreadFunc func);

	// �߳�����
	~Thread();

	// �����������̺߳���
	void start();

	// ��ȡ�߳�id
	int getThreadId() const;
private:
	ThreadFunc Func_;
	static int generateId_;
	int threadId_; // �����߳�id
};


//	����������
class Task {
	//	�û������Զ���������������ͣ���Task�̳� ��дrun������ʵ���Զ���������
public:
	Task();
	~Task() = default;
	virtual Any run() = 0;
	void exec();
	void setResult(Result* res);
private:
	Result *result_; // ����ʹ��Result������ָ�� �ᵼ��ѭ������ ����Result����Ҳ��Task����ʱ�䳤
};

//	�̳߳�����
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	//�����̳߳صĹ���ģʽ
	void setTheadPoolMode(ThreadPoolMode Mode);


	//����task�������������ֵ 
	void setTaskQueMaxThreshHold(int threshhold);

	//���̳߳��ύ���� ��Ϊ������д�ŵľ�������ָ����� ���Բ���Ҳ������ָ�����
	Result submitTask(std::shared_ptr<Task> sp);

	//�����̳߳�
	void start(int initThreadSize = 4);

	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold);

	// ��ֹ�̳߳صĿ�����ֵ ����̫�� û�б�Ҫ���临��
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����߳�Ҫִ�еĺ���
	void threadFunc(int thread);

	// ���pool������״̬
	bool checkRunningState() const;
private:
	//std::vector<std::unique_ptr<Thread>> threads_; //�߳��б�(�̶߳���) 
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	// ����fixedģʽ����Ҫ�����̰߳�ȫģʽ ��Ϊ�߳������տ�ʼ���ǹ̶��� 
	// ����cachedģʽ��˵ ����̵߳����vector���б仯 Ҫ�����̰߳�ȫ
	// ��������ָ�� ������Դй¶ ��Ϊ�������ָ��Ļ� ָ�����Ӧ��û���������� ���Իᵼ��ָ��ָ�����һƬ����δ�ͷ�

	size_t initThreadSize_; //��ʼ���߳�����
	std::atomic_uint idleThreadSize_; // ��¼�����̵߳�����
	std::atomic_uint curThreadSize_; //��¼��ǰ�̳߳������̵߳�������
	int threaSizeThreshHold; //�߳�����������ֵ
	std::queue<std::shared_ptr<Task>> taskQue_;//�������
	// Ϊʲô��ŵ�����������������ָ�룿
	//�û����п��ܴ���һ����ʱ������������ �������û����ύ���������֮�� �����������ͱ��ͷ���
	//��������������ܻ����ҵ���������У���ʱ����������ڵ���������������д����ͻ���ִ���
	// ��������Ҫ�ܹ��Զ��ͷŶ������Դ

	std::atomic_uint taskSize_; //������������������
	// Ϊʲôʹ��atomic_uint��
	// ��Ϊ������Ҫ��¼����ĸ��� �̳߳�����һ������ ��������1 �û��ύһ������ ��������1
	// �����ڲ�ͬ�߳��ж���һ���������мӼӼ��������̰߳�ȫ��,��������û��Ҫ��ÿ�ν��мӼӼ���
	// ���л��������л��� �����ᵼ��Ч�ʺ��� �������ǿ���ֱ��ʹ��atomic_uint
	
	// ʲô��atomic_uint��

	int taskQueMaxThreshHold_; //�����������������ֵ ������п϶������ܷǳ��� �û�������������������ֵ

	std::mutex taskQueMtx; //��֤������е��̰߳�ȫ

	std::condition_variable notFull_; //��ʾ������в���
	std::condition_variable notEmpty_; //��ʾ������в���
	// Ϊʲô������������������
	std::condition_variable exitCond_; // �ȴ��߳���Դȫ������
	ThreadPoolMode poolMode_; //��ǰ�̳߳صĹ���ģʽ

	std::atomic_bool isPoolRuning;		 // ��ʾ��ǰ�̳߳ص�����״̬
};

#endif

