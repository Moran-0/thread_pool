#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
	ThreadPool(size_t numThreads);
	~ThreadPool();

	template <class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
	    -> std::future<typename std::result_of<F(Args...)>::type>;

private:
	std::vector<std::thread> workers;        // 工作线程池
	std::queue<std::function<void()>> tasks; // 任务队列

	/*同步 */
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

inline ThreadPool::ThreadPool(size_t numThreads)
    : stop(false) {
	for (size_t i = 0; i < numThreads; ++i) {
		workers.emplace_back([this]() {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->queue_mutex);
					condition.wait(lock, [this]() {
						return this->stop || !tasks.empty();
					});
					if (stop || tasks.empty()) {
						return;
					}
					task = std::move(tasks.front());
					tasks.pop();
				}
				task();
			}
		});
	}
}

template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
	using return_type = typename std::result_of<F(Args...)>::type;
	if (stop) {
		throw std::runtime_error("线程池已停止！");
	}
	// 任务被推入队列后，可能在 enqueue 函数返回后很久才被线程执行。shared_ptr
	// 通过引用计数确保 packaged_task 在所有引用（包括 lambda
	// 捕获）消失前不会被销毁，避免悬空指针或未定义行为。 如果不使用
	// shared_ptr，可能导致任务执行时对象已销毁的风险
	auto task = std::make_shared<std::packaged_task<return_type()>>(
	    std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	std::future<return_type> future_result = task->get_future();
	{
		std::unique_lock<std::mutex> lock(this->queue_mutex);
		tasks.emplace([task]() { (*task)(); });
	}
	condition.notify_one();
	return future_result;
}

inline ThreadPool::~ThreadPool() {
	{
		std::unique_lock<std::mutex> lock(this->queue_mutex);
		stop = true;
	}
	condition.notify_all(); // 通知所有线程完成任务
	// 手动join所有工作线程确保线程安全退出
	// join可以阻塞主线程，同步销毁工作线程，防止主线程退出后工作线程仍在运行（会访问已销毁的成员变量），
	for (auto& worker : workers) {
		worker.join();
	}
}