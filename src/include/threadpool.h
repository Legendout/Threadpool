#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <sys/syscall.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

namespace threadpool {

class ThreadPool {

public:
  ThreadPool(uint32_t capacity);
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;
  ~ThreadPool();

  template <typename Func, typename... Args,
            typename RtrnType = typename std::result_of<Func(Args...)>::type>
  auto make_task(Func &&func, Args &&...args)
      -> std::packaged_task<RtrnType(void)>;

  template <typename Func, typename... Args,
            typename RtrnType = typename std::result_of<Func(Args...)>::type>
  auto enqueue(Func &&func, Args &&...args) -> std::future<RtrnType>;

  auto before_task_hook() -> void;

  auto after_task_hook() -> void;

  auto wait_and_stop() -> void;

  template <typename Func, typename... Args>
  auto spawn(Func &&func, Args &&...args) -> void;

private:
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;

  std::mutex mutex_;
  std::condition_variable cv_;

  std::atomic<bool> stop_pool_{false};
  std::atomic<uint32_t> activate_threads_{0};
  std::atomic<uint32_t> capacity_;
};

} // namespace threadpool
