// [ QSapecNG ] progress and cancelling of a running mrt enumeration.
// Part of QSapecNG, GPL v3 or later.

#ifndef MRT_PROGRESS_H
#define MRT_PROGRESS_H

#include <atomic>
#include <cmath>
#include <exception>

#include "boost-sapecng/mrt_pool.hpp"


namespace boost
{


// [ QSapecNG ] progress and cancelling of a running enumeration, for the
// thread that runs it. The search is a binary tree of choices (an edge in
// or out): each call gets a share of the whole, half for each of its two
// sub-searches, and adds what it does not hand on when it is done. The sum
// rises steadily to 1, whatever the number of trees.
//
// With a pool, parts of the search run on other threads, each with an
// mrt_progress of its own: their shares meet in `total`.
struct mrt_progress
{
  double done = 0.;
  double weight = 1.;
  std::atomic<bool>* cancel = nullptr;
  std::atomic<int>* permille = nullptr;   // done, in 1/1000 (read elsewhere)

  // [ QSapecNG ] multi-core (all optional)
  mrt_pool* pool = nullptr;               // where to hand work on
  std::atomic<long long>* total = nullptr;   // done by all, in 1 / unit
  std::atomic<long long>* trees = nullptr;   // trees found by all
  long long term_limit = 0;   // the most distinct terms kept (0: no limit)
  long long found = 0;                       // by this thread, not yet in
  unsigned ticks = 0;

  static constexpr double unit = 1099511627776.;   // 2^40

  // `done` was just raised
  inline void advanced()
  {
    if(!total) {
      if(trees && (++ticks & 63) == 0)
        flush();
      if(permille)
        permille->store(int(done * 1000.), std::memory_order_relaxed);
    } else if((++ticks & 63) == 0) {
      flush();
    }
  }

  // passes this thread's part on to the total
  inline void flush()
  {
    if(trees && found) {
      trees->fetch_add(found, std::memory_order_relaxed);
      found = 0;
    }
    if(!total)
      return;
    const long long add = std::llround(done * unit);
    done -= double(add) / unit;
    const long long sum =
      total->fetch_add(add, std::memory_order_relaxed) + add;
    if(permille)
      permille->store(int(double(sum) / unit * 1000.),
        std::memory_order_relaxed);
  }
};

struct mrt_cancelled: public std::exception
{
  const char* what() const noexcept { return "cancelled"; }
};

inline mrt_progress*& mrt_current_progress()
{
  static thread_local mrt_progress* p = nullptr;
  return p;
}

// [ QSapecNG ] makes p the current progress of this thread while alive
// (a thread of the pool runs parts of several searches, one after another)
struct mrt_progress_scope
{
  explicit mrt_progress_scope(mrt_progress* p)
    : previous_(mrt_current_progress())
    { mrt_current_progress() = p; }
  ~mrt_progress_scope()
  {
    if(mrt_current_progress())
      mrt_current_progress()->flush();
    mrt_current_progress() = previous_;
  }
  mrt_progress_scope(const mrt_progress_scope&) = delete;
  mrt_progress_scope& operator=(const mrt_progress_scope&) = delete;
private:
  mrt_progress* previous_;
};


}


#endif // MRT_PROGRESS_H
