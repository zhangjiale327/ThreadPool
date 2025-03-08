#include "threadpool.h"
#include <thread>
const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
// ��ʼ��������е�������ֵ���߳���������ֵ
const int THREAD_MAX_IDLE_TIME = 5; //��λ����
////////////////////////////////////////////ThreadPool����ʵ��
// �̳߳صĹ��캯��
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
// �̳߳ص�����
ThreadPool::~ThreadPool() {
	// ��Ϊ�����ʱ������û��new���� ���Ծ�ûʲô������������
	isPoolRuning = false;
	//�ȴ��̳߳��������е��̷߳��� 
}

//�����̳߳صĹ���ģʽ
void ThreadPool::setTheadPoolMode(ThreadPoolMode Mode) {
	if (checkRunningState()) {
		return;
	}
	this->poolMode_ = Mode;
}


//����task�������������ֵ 
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {
	this->taskQueMaxThreshHold_ = threshhold;
}

//���̳߳��ύ����  �û����øýӿ� �������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
	// ��Ϊ���û����߳��б�һ������������������ �����û��������� ���߳��б���������
	/// ����ͬʱ�����Ļ� �϶��ǲ������ ����Ҫ�����߳�ͬ��

	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx);
	// �̵߳�ͨ�� �ȴ���������п��� ����п��� ��������뵽���������
	// ��Ϊ�·��������� ������п϶������� ��notEmpty�Ͻ���֪ͨ
	// wait:һֱ�� �ȵ��������������е�ǰ�߳�
	// wait_for: ���ȶ೤�¼� ��������Ͳ����� ����ֵΪBOOL
	// wait_until: ���㵽�¸�����һ �����ȴ����¼� ����һ��ʱ����ֹ�ڵ�
	/*while (taskQue_.size() == taskQueMaxThreshHold_) {
		notFull_.wait(lock);
	}*/
	// ���д���������whileѭ���ǵȼ۵�
	// �û��ύ���� ���������������1s�������ж��ύ����ʧ�ܣ����أ�����Ƿ���Ľ��� �����û���̨û�п���Ķ���
	// �������˼ ��������û��ύ������̳߳ر����ԭ�����������һֱ��Ϊ�� �����û�����������
	// ����û����õ����� ���Լ����ж����һ����û���ύ�ɹ� �򷵻��ύʧ�� 
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool
		{return taskQue_.size() < static_cast<size_t>(taskQueMaxThreshHold_); })) {
		std::cerr << "task queue is full,submit task fail." << std::endl;
		// wait_for����ֵ���Ϊfalse�Ļ� ��˵���ȴ�һ����������û������
		return Result(sp,false);
	}
	// ���������������Ļ� ������������ �ͷ�ǰ����� ���Ҹı䵱ǰ�߳�״̬ ����->����
// ����п��࣬��������뵽���������
	taskQue_.emplace(sp);
	taskSize_++; 

	// ��Ϊ�·�������������п϶������ˣ���notEmpty_�Ͻ���֪ͨ ֪ͨ�����ߣ��̣߳�
	notEmpty_.notify_all();

	// cachedģʽ ������ȽϽ��� ������ С��������� ��Ҫ�������������Ϳ����̵߳����� �ж��Ƿ���Ҫ�����µ��߳�
	if (poolMode_ == ThreadPoolMode::MODE_CACHED 
		&& taskSize_>idleThreadSize_ 
		&& curThreadSize_< threaSizeThreshHold) {
		// �������߳�
		std::cout << ">>> create new thread" << std::this_thread::get_id() << " exit!" << std::endl;
		// ����thread�̶߳����ʱ�� ���̺߳�������thred�̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		// make_unique��c++14�����
		int threadId = ptr->getThreadId();
		threads_.emplace(threadId, std::move(ptr));
		// �����߳�
		threads_[threadId]->start();
	}
	// ���������Result����
	return Result(sp,true);
}

//�����̳߳�
void ThreadPool::start(int initThreadSize) {
	// ��¼��ʼ���߳�����
	this->initThreadSize_ = initThreadSize;
	this->curThreadSize_ = initThreadSize;
	// �����̳߳ص�����״̬
	this->isPoolRuning = true;
	//�����̶߳��� �Ȱ��̼߳��뵽�߳��б���
	for (int i = 0; i < this->initThreadSize_; i++) {
		// ����thread�̶߳����ʱ�� ���̺߳�������thred�̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		// make_unique��c++14�����
		int threadId = ptr->getThreadId();
		threads_.emplace(threadId,std::move(ptr));
		// ��Ϊunique������������ ��Ϊ�������л���ö���Ŀ������� ����������Ҫת����������ʹ��Ȩ
		// ��ʵ��ת�Ƶ��β�
	}
	// ���������߳�	std::vector<Thread*> threads_;  //�߳��б�(�̶߳���) 
	for (int i = 0; i < initThreadSize_; i++) {
		// �������̶߳�����ִ��ÿ������Ҫִ�е��̺߳���
		threads_[i]->start(); // ÿ���߳���Ҫȥִ��һ���̺߳����� 
		idleThreadSize_++; //��¼�����̵߳�����
	}
}


// �����߳�ִ�к���   �̳߳ص������̴߳��������������������
void ThreadPool::threadFunc(int thread) { //�̺߳������أ���Ӧ���߳�Ҳ�ͽ�����
	/*std::cout << "begin threadFunc  tid : "
		<< std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc  tid : "
		<< std::this_thread::get_id() << std::endl;*/
	auto lastime = std::chrono::high_resolution_clock().now();
	for (;;) {
		std::shared_ptr<Task> task;
		// �Ȼ�ȡ��
		{
			std::unique_lock<std::mutex> lock(taskQueMtx);
			std::cout << "tid: " << std::this_thread::get_id() << "���Ի�ȡ����..." << std::endl;
			// cashedģʽ�� �п����Ѿ������˺ܶ���̵߳��ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳̽������յ�
			// ������initThreadSize_�������߳̽��л��գ�
			// ��ǰʱ��-��һ���߳�ִ�е�ʱ��>60s
			if (poolMode_ == ThreadPoolMode::MODE_CACHED) {
				// ÿһ���з���һ��  ��ô���֣���ʱ���أ���������ִ�з���
				while (taskQue_.size() == 0) {
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_>initThreadSize_) {
							// ��ʼ�����߳�
							// ��¼�߳���������ر�����ֵ�޸�
							// ���̶߳�����߳��б�������ɾ�� û�а취ƥ��threadFuns��Ӧ�Ǹ�thread����
							// �ҵ�threadid��Ӧ��thread���� ��������ɾ��
							threads_.erase(_threadid); 
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "threadid " << std::this_thread::get_id() << " exit" << std::endl;
							return;
						}
					}
				}
			}
			// �ȴ�notEmpty����
			notEmpty_.wait(lock, [&]()->bool {return static_cast<bool>(taskQue_.size() > 0); });
			// �����������ȡ��һ���������
			idleThreadSize_--;
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			std::cout << "tid: " << std::this_thread::get_id() << "��ȡ����ɹ�..." << std::endl;
			// �����Ȼ��ʣ������ ����֪ͨ�������߳�ִ������ ���ǵ�һ�����з�� ���滹���з ֪ͨ������Ҳ��
			if (taskQue_.size() > 0) {
				notEmpty_.notify_all();
			}
			// ˳��Ҳ֪ͨһ���ύ�������� ��ǰ�̳߳ز��� �����ύ����
			notFull_.notify_all();
		} // ��Ϊ���ǲ��ܵ�ǰ�̻߳�ȡ����֮�� ��һֱ������߳�������֮�� ���ͷ������
		 // �����ᵼ���κ�ʱ�� ֻ��һ���߳���ִ��
		// ��ǰ�̸߳���ִ���������
		if (task != nullptr) {
			// �ж�һ�� ��ֹ�����쳣��� ���³������
			task->exec(); // ִ������ ��������ķ���ֵsetVal��������Result
			lastime = std::chrono::high_resolution_clock().now(); //�����߳�ִ���������ʱ��
		}
		idleThreadSize_++;
	}
}

// �����̳߳�cachedģʽ���߳���ֵ
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

/////////////////////////////////////////////////////////////////////////////////Thread����ʵ��
// �̹߳���
Thread::Thread(ThreadFunc func)
	:Func_(func)
	,threadId_(generateId_++) 
{

}
int Thread::generateId_ = 0;
// �߳�����
Thread::~Thread() {

}

// �����������̺߳���
void Thread::start() {
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(Func_,threadId_); //c++11��˵ ��Ϊ�̶߳���t ���̺߳���Func_
	t.detach(); //���÷����߳�
	//ΪʲôҪ���÷����̣߳�
	// ��Ϊ���������Ļ� �̶߳���t��ִ���̺߳���Func_���߳��Ǿ��а󶨹�ϵ��
	// �����start����ִ����� �̶߳���t�뿪������ �̶߳���t�ᱻ���� Ҳ����ִ���̺߳���������߳�Ҳ��ֹ��
	// ����Ϊ��ʹ�̶߳���Ӱ������ִ�е��߳� ���������̷߳��� �������
}

int Thread::getThreadId() const {
	return threadId_;
}
///////////////////////////////////////////////////////////////////////////////// Semaphore����ʵ��
//��ȡһ���ź�����Դ
void  Semaphore::wait() {
	std::unique_lock<std::mutex> lock(mtx_);
	// �ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
	cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
	resLimit_--;
}

// ����һ���ź�����Դ
void Semaphore::post() {
	std::unique_lock<std::mutex> lock(mtx_);
	resLimit_++;
	cond_.notify_all();
}


///////////////////////////////////////////////////////////////////////////////////Result����ʵ��
Result::Result(std::shared_ptr<Task> task,bool isValid):isValid_(isValid),task_(task){
	task_->setResult(this); //��������ó�ʼ��Task����
}

//��ȡ����ֵ���
Any Result::get(){
	if (!isValid_) {
		return " ";
	}
	sem_.wait(); //task�������û��ִ���꣬����������û����߳�
	return std::move(any_);
}

void Result::setVal(Any any) {
	// �洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post(); // �Ѿ���ȡ��������ķ���ֵ
}


///////////////////////////////////////////////////////////////////////////////////Task����ʵ��

Task::Task():result_(nullptr) {

}

void Task::exec() {
	if (result_ != nullptr) {
		result_->setVal(run()); //���ж�̬����
	}
}

void Task::setResult(Result* res) {
	this->result_ = res;
}