#include "include/threadpool.h"

namespace threadpool {

ThreadPool::ThreadPool(uint32_t capacity)
    : capacity_(capacity), activate_threads_(0), stop_pool_(false) {
  for (size_t i = 0; i < capacity; ++i) {
    threads_.emplace_back([this]() {
      while (true) {
        std::function<void(void)> task;
        {
          std::unique_lock<std::mutex> unique_lock(mutex_);
          auto predicate = [this]() -> bool {
            return (stop_pool_) || !(tasks_.empty());
          };
          cv_.wait(unique_lock, predicate);
          // will unlock the mutex and wait for the
          // condition variable to be notified or until the
          // predicate returns true
          if (stop_pool_ && tasks_.empty()) {
            return;
          }
          task = std::move(tasks_.front());
          tasks_.pop();
          before_task_hook();
        }
        task();
        {
          std::lock_guard<std::mutex> lock_guard(mutex_);
          after_task_hook();
        }
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock_guard(mutex_);
    stop_pool_ = true;
  }
  cv_.notify_all();
  for (auto &thread : threads_) {
    thread.join();
  }
}

template <typename Func, typename... Args, typename RtrnType>
auto ThreadPool::make_task(Func &&func, Args &&...args)
    -> std::packaged_task<RtrnType(void)> {
  auto binded =
      std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
  return std::packaged_task<RtrnType(void)>(binded);
}

template <typename Func, typename... Args, typename RtrnType>
auto ThreadPool::enqueue(Func &&func, Args &&...args) -> std::future<RtrnType> {
  auto task = make_task(std::forward<Func>(func), std::forward<Args>(args)...);
  auto future = task.get_future();

  auto task_ptr = std::make_shared<decltype(task)>(std::move(task));
  {
    std::lock_guard<std::mutex> lock_guard(mutex_);
    if (stop_pool_) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }
    auto predicate = [task_ptr]() -> void { task_ptr->operator()(); };
    tasks_.emplace(predicate);
  }
  cv_.notify_one();
  return future;
}

auto ThreadPool::before_task_hook() -> void { activate_threads_++; }

auto ThreadPool::after_task_hook() -> void {
  activate_threads_--;
  if (activate_threads_ == 0 && tasks_.empty()) {
    stop_pool_ = true;
    cv_.notify_one();
  }
}

auto ThreadPool::wait_and_stop() -> void {
  std::unique_lock<std::mutex> unique_lock(mutex_);
  auto predicate = [this]() { return stop_pool_ && tasks_.empty(); };
  cv_.wait(unique_lock, predicate);
}

template <typename Func, typename... Args>
auto ThreadPool::spawn(Func &&func, Args &&...args) -> void {
  if (activate_threads_ < capacity_) {
    enqueue(func, args...);
  } else {
    func(args...);
  }
}

} // namespace threadpool

const std::string &add_func(const std::string &name) {
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << std::to_string(syscall(SYS_gettid)) + "-" + name << std::endl;
  return name;
}

void test01() {
  threadpool::ThreadPool tp(5);
  std::vector<std::future<const std::string &>> vec;
  for (int i = 0; i < 4; ++i) {
    vec.emplace_back(tp.enqueue(add_func, "haha" + std::to_string(i)));
  }

  for (auto &future : vec) {
    std::cout << future.get() << std::endl;
  }
}