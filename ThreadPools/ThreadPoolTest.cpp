#include "ThreadPool.h"
#include <iostream>

int main() {
	auto hardware_threads = std::thread::hardware_concurrency();

	ThreadPool thdPools(std::thread::hardware_concurrency());
	std::vector<std::future<int>> results;
	for (uint32_t i = 0; i < hardware_threads * 2; ++i) {
		results.emplace_back(thdPools.enqueue([i]() {
			std::cout << "Hello " << i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::cout << "World " << i << std::endl;
			return static_cast<int>(i * i);
		}));
	}
	for (auto&& res : results) {
		std::cout << res.get() << std::endl;
	}
	return 0;
}