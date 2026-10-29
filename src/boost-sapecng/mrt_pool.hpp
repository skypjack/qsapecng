// [ QSapecNG ] a small pool of worker threads for the solver.
// Part of QSapecNG, GPL v3 or later.

#ifndef MRT_POOL_H
#define MRT_POOL_H

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>


namespace boost
{


// [ QSapecNG ] A fixed number of threads share all the work of a solve: the
// outputs (roots) and the pieces they are split into (tasks). The thread
// that makes the pool is one of them: it works while it waits, so a pool
// of n uses n cores at most. Whoever waits for a group of tasks runs queued
// tasks meanwhile, so waiting never leaves a core idle (nor deadlocks).
class mrt_pool
{

public:
  // tasks one can wait for together
  class group
  {
  public:
    group() { }
    group(const group&) = delete;
    group& operator=(const group&) = delete;
  private:
    friend class mrt_pool;
    std::atomic<long> pending_{0};
    std::exception_ptr error_;   // the first one thrown by a task
  };

  explicit mrt_pool(unsigned threads)
    : threads_(std::max(1u, threads))
  {
    for(unsigned i = 1; i < threads_; ++i)
      workers_.emplace_back([this]() { work(); });
  }

  ~mrt_pool()
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();
    for(std::thread& t : workers_)
      t.join();
  }

  mrt_pool(const mrt_pool&) = delete;
  mrt_pool& operator=(const mrt_pool&) = delete;

  inline unsigned size() const { return threads_; }

  // a piece of a running job
  void submit(group& g, std::function<void()> f)
    { push(g, std::move(f), false); }

  // a whole job (an output): only the pool's own threads and wait_all()
  // start one, a job waiting for its pieces does not start another
  void submit_root(group& g, std::function<void()> f)
    { push(g, std::move(f), true); }

  // worth splitting work: some thread may soon be without any
  inline bool hungry() const
  {
    return threads_ > 1
      && queued_.load(std::memory_order_relaxed) < std::size_t(threads_);
  }

  // until the tasks of g are done, runs queued tasks; returns what the
  // first failing task of g threw (or nothing)
  std::exception_ptr wait(group& g)
    { return wait(g, false); }

  // as wait(), also starting queued roots
  std::exception_ptr wait_all(group& g)
    { return wait(g, true); }

private:
  struct task
  {
    group* g = nullptr;
    std::function<void()> f;
  };

  void push(group& g, std::function<void()> f, bool root)
  {
    g.pending_.fetch_add(1);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      (root ? roots_ : tasks_).push_back(task{ &g, std::move(f) });
      queued_.store(roots_.size() + tasks_.size(), std::memory_order_relaxed);
    }
    cv_.notify_all();
  }

  // under the lock; the pieces of running jobs first, so they end soon
  bool pop(task& t, bool roots)
  {
    std::deque<task>* q = !tasks_.empty() ? &tasks_
      : (roots && !roots_.empty() ? &roots_ : nullptr);
    if(!q)
      return false;
    t = std::move(q->front());
    q->pop_front();
    queued_.store(roots_.size() + tasks_.size(), std::memory_order_relaxed);
    return true;
  }

  void run(task& t)
  {
    try {
      t.f();
    } catch(...) {
      std::lock_guard<std::mutex> lock(mutex_);
      if(!t.g->error_)
        t.g->error_ = std::current_exception();
    }
    t.f = nullptr;   // what it holds goes before anyone is told
    if(t.g->pending_.fetch_sub(1) == 1) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

  std::exception_ptr wait(group& g, bool roots)
  {
    for(;;) {
      task t;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() {
            return g.pending_.load() == 0 || !tasks_.empty()
              || (roots && !roots_.empty());
          });
        if(g.pending_.load() == 0) {
          std::exception_ptr e = g.error_;
          g.error_ = nullptr;
          return e;
        }
        pop(t, roots);
      }
      run(t);
    }
  }

  void work()
  {
    for(;;) {
      task t;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() {
            return stop_ || !tasks_.empty() || !roots_.empty();
          });
        if(!pop(t, true))
          return;   // stopping, nothing left
      }
      run(t);
    }
  }

  const unsigned threads_;
  std::vector<std::thread> workers_;
  std::deque<task> tasks_, roots_;
  std::atomic<std::size_t> queued_{0};
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;

};


}


#endif // MRT_POOL_H
