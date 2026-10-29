/*
    SapecNG - Next Generation Symbolic Analysis Program for Electric Circuit
    Copyright (C) 2009, Michele Caini; 2026, QSapecNG contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


//  The common spanning trees of two graphs on the same edges, after
//  Minty, Read and Tarjan (Electron. Lett. 19(9), 1983): the search of
//  boost-sapecng/mrt.hpp, making the same choices in the same order, so
//  the trees come out in the same order. It works on edge numbers and
//  plain arrays: a cycle test is a union-find lookup, not a graph search,
//  and nothing is allocated per step. Unlike mrt.hpp it never hands on
//  sets that are not trees (its "fat trees"): the bridges forced into a
//  tree are checked together, not one at a time.


#ifndef TREESEARCH_H
#define TREESEARCH_H


#include "boost-sapecng/mrt_pool.hpp"
#include "boost-sapecng/mrt_progress.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>


namespace sapecng
{


// the two graphs: edge j joins iu[j]-iv[j] in one, vu[j]-vv[j] in the other
struct tree_graphs
{
  int vertices = 0;
  int edges = 0;
  std::vector<int> iu, iv, vu, vv;
};


// where a tree lies in the search: the choices made on the way to it (bit
// d is the choice at depth d, 0 = the edge in, 1 = out), most significant
// first. The trees come in the order of these words compared as numbers,
// whichever thread finds them.
typedef std::vector<std::uint64_t> tree_position;

inline bool tree_before(const tree_position& a, const tree_position& b)
{
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}


// Calls sink.tree(inL, position) for every tree (inL[j]: edge j in it).
// The sink makes a fresh one for a part of the search (Sink::part()) and
// takes one back (Sink::merge(Sink&&), under a lock); the order parts are
// merged in is not fixed. With a pool in the progress (see mrt_progress),
// parts run on other threads. Throws boost::mrt_cancelled when cancelled.
template <class Sink>
class tree_search
{

public:
  // share: hand on parts with at least this many edges left to choose
  // (0: half the tree's edges; tests use 1 to hand on as much as they can)
  explicit tree_search(const tree_graphs& g, int share = 0): g_(g)
  {
    words_ = std::size_t(g.edges + 2) / 64 + 1;
    // parts handed on are big ones (half the tree or more still to
    // choose): each costs a state and a merge of its terms
    share_ = share > 0 ? share : std::max(4, (g.vertices - 1) / 2);
  }

  void run(const std::vector<bool>& forced, Sink& sink,
    boost::mrt_progress* progress)
  {
    walker root(*this, progress);
    root.sink.reset(new Sink(sink.part()));
    for(int j = 0; j < g_.edges; ++j)
      root.inL[j] = forced[j] ? 1 : 0;
    root.count();

    boost::mrt_pool::group group;
    group_ = &group;
    std::exception_ptr error;
    try {
      if(root.start())
        root.rec(1., 0);
      else
        root.p.done += 1.;
      root.p.flush();
      merge(*root.sink);
    } catch(...) {
      error = std::current_exception();
      if(progress && progress->cancel)
        progress->cancel->store(true);   // the parts elsewhere stop too
    }
    if(pool()) {
      std::exception_ptr other = pool()->wait(group);
      if(!error)
        error = other;
    }
    group_ = nullptr;
    if(error)
      std::rethrow_exception(error);
    std::lock_guard<std::mutex> lock(mutex_);
    sink.merge(std::move(*merged_));
    merged_.reset();
  }

  // the number of trees run() would find, estimated by random paths down
  // the search (Knuth's estimator: each path weighs the product of the
  // choices it passed). Paths for `seconds`, on every thread of the pool
  // (longer when that gave few, up to four times), or until the estimate
  // is within 0.3 %.
  // Returns the mean; *error: its standard error
  double estimate(const std::vector<bool>& forced, double seconds,
    boost::mrt_pool* pool = nullptr, double* error = nullptr)
  {
    if(error)
      *error = 0.;
    {
      walker w(*this, nullptr);
      for(int j = 0; j < g_.edges; ++j)
        w.inL[j] = forced[j] ? 1 : 0;
      w.count();
      if(!w.start())
        return 0.;
    }
    std::mutex mutex;
    double sum = 0., squares = 0.;
    long long paths = 0;
    bool done = false;
    const auto begin = std::chrono::steady_clock::now();
    auto work = [&](std::uint64_t seed) {
        walker w(*this, nullptr);
        for(int j = 0; j < g_.edges; ++j)
          w.inL[j] = forced[j] ? 1 : 0;
        w.count();
        w.start();
        const std::vector<char> inL = w.inL, del = w.del;
        const int in = w.in;
        for(;;) {
          double s = 0., q = 0.;
          for(int k = 0; k < 64; ++k) {
            w.inL = inL;
            w.del = del;
            w.in = in;
            const double x = w.probe(seed);
            s += x;
            q += x * x;
          }
          std::lock_guard<std::mutex> lock(mutex);
          sum += s;
          squares += q;
          paths += 64;
          if(done)
            return;
          const double mean = sum / paths;
          const double se = std::sqrt(std::max(0.,
            squares / paths - mean * mean) / paths);
          // enough: precise, or the time is up (with paths enough on a
          // slow computer, up to four times the time)
          const double spent = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - begin).count();
          if((paths >= 2048 && se <= 0.003 * mean) || paths >= 50000000
              || (spent > seconds && paths >= 16384) || spent > 4 * seconds)
          {
            done = true;
            return;
          }
        }
      };
    if(pool && pool->size() > 1) {
      boost::mrt_pool::group group;
      for(unsigned i = 1; i < pool->size(); ++i)
        pool->submit(group, [&work, i]() {
            work(0x9e3779b97f4a7c15ull * (i + 1));
          });
      work(0x9e3779b97f4a7c15ull);
      pool->wait(group);
    } else {
      work(0x9e3779b97f4a7c15ull);
    }
    const double mean = sum / paths;
    if(error)
      *error = std::sqrt(std::max(0., squares / paths - mean * mean) / paths);
    return mean;
  }

private:
  boost::mrt_pool* pool() const
    { return progress_ ? progress_->pool : nullptr; }

  void merge(Sink& part)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!merged_)
      merged_.reset(new Sink(std::move(part)));
    else
      merged_->merge(std::move(part));
  }

  // one thread's share of the search, with a state of its own
  struct walker
  {
    walker(tree_search& s, boost::mrt_progress* progress)
      : s(s), g(s.g_)
    {
      const int V = g.vertices, M = g.edges;
      inL.assign(M, 0);
      del.assign(M, 0);
      tree.assign(M, false);
      position.assign(s.words_, 0);
      // per depth (at most one step per edge, and the start)
      di.assign(std::size_t(M + 2) * V, 0);
      dv.assign(std::size_t(M + 2) * V, 0);
      list.assign(std::size_t(M + 2) * (2 * M + 1), 0);
      adjStart.assign(V + 1, 0);
      adj.assign(2 * M + 1, 0);
      pos.assign(V + 1, 0);
      disc.assign(V, 0);
      low.assign(V, 0);
      stack.reserve(V + 1);
      bridgeList.reserve(2 * M + 1);
      if(progress) {
        p.cancel = progress->cancel;
        p.permille = progress->permille;
        p.pool = progress->pool;
        p.total = progress->total;
        p.trees = progress->trees;
        if(!s.progress_)
          s.progress_ = progress;
      }
    }

    tree_search& s;
    const tree_graphs& g;
    boost::mrt_progress p;
    std::unique_ptr<Sink> sink;
    std::vector<char> inL, del;
    std::vector<bool> tree;
    tree_position position;
    int in = 0;   // edges in inL
    unsigned ticks = 0;

    std::vector<int> di, dv, list;
    std::vector<int> adjStart, adj, pos, disc, low, bridgeList;
    struct frame { int x, via, k; };
    std::vector<frame> stack;

    void count()
      { in = int(std::count(inL.begin(), inL.end(), 1)); }

    // union-find, one set of arrays per depth
    static int find(int* p, int x)
    {
      while(p[x] != x) {
        p[x] = p[p[x]];
        x = p[x];
      }
      return x;
    }
    void build(int* p, const std::vector<int>& u, const std::vector<int>& v)
    {
      for(int x = 0; x < g.vertices; ++x)
        p[x] = x;
      for(int j = 0; j < g.edges; ++j)
        if(inL[j])
          p[find(p, u[j])] = find(p, v[j]);
    }
    // adds edge j to both, false if it closes a cycle in one of them
    bool join(int* pi, int* pv, int j)
    {
      const int a = find(pi, g.iu[j]), b = find(pi, g.iv[j]);
      const int c = find(pv, g.vu[j]), d = find(pv, g.vv[j]);
      if(a == b || c == d)
        return false;
      pi[a] = b;
      pv[c] = d;
      return true;
    }
    bool closes(int* pi, int* pv, int j)
    {
      return find(pi, g.iu[j]) == find(pi, g.iv[j])
        || find(pv, g.vu[j]) == find(pv, g.vv[j]);
    }

    // the bridges of a graph without the deleted edges, appended
    void bridges(const std::vector<int>& u, const std::vector<int>& v)
    {
      const int V = g.vertices, M = g.edges;
      std::fill(adjStart.begin(), adjStart.end(), 0);
      for(int j = 0; j < M; ++j)
        if(!del[j]) {
          ++adjStart[u[j] + 1];
          ++adjStart[v[j] + 1];
        }
      for(int x = 0; x < V; ++x)
        adjStart[x + 1] += adjStart[x];
      std::copy(adjStart.begin(), adjStart.end(), pos.begin());
      for(int j = 0; j < M; ++j)
        if(!del[j]) {
          adj[pos[u[j]]++] = j;
          adj[pos[v[j]]++] = j;
        }
      std::fill(disc.begin(), disc.end(), -1);
      int t = 0;
      for(int r = 0; r < V; ++r) {
        if(disc[r] >= 0)
          continue;
        disc[r] = low[r] = t++;
        stack.clear();
        stack.push_back(frame{ r, -1, adjStart[r] });
        while(!stack.empty()) {
          frame& f = stack.back();
          if(f.k < adjStart[f.x + 1]) {
            const int e = adj[f.k++];
            if(e == f.via)
              continue;
            const int y = (u[e] == f.x) ? v[e] : u[e];
            if(disc[y] < 0) {
              disc[y] = low[y] = t++;
              stack.push_back(frame{ y, e, adjStart[y] });
            } else if(disc[y] < low[f.x]) {
              low[f.x] = disc[y];
            }
          } else {
            const frame done = f;
            stack.pop_back();
            if(!stack.empty()) {
              frame& parent = stack.back();
              if(low[done.x] < low[parent.x])
                low[parent.x] = low[done.x];
              if(low[done.x] > disc[parent.x])
                bridgeList.push_back(done.via);
            }
          }
        }
      }
    }

    int components(const std::vector<int>& u, const std::vector<int>& v,
      int* p)
    {
      for(int x = 0; x < g.vertices; ++x)
        p[x] = x;
      for(int j = 0; j < g.edges; ++j)
        if(!del[j])
          p[find(p, u[j])] = find(p, v[j]);
      int c = 0;
      for(int x = 0; x < g.vertices; ++x)
        if(find(p, x) == x)
          ++c;
      return c;
    }

    // the edges every tree has, and those none can have; false: no tree
    bool start()
    {
      int* pi = di.data();
      int* pv = dv.data();
      bridgeList.clear();
      bridges(g.iu, g.iv);
      bridges(g.vu, g.vv);
      for(int e : bridgeList)
        inL[e] = 1;
      count();
      // a forest in both
      for(int x = 0; x < g.vertices; ++x)
        pi[x] = pv[x] = x;
      for(int j = 0; j < g.edges; ++j)
        if(inL[j] && !join(pi, pv, j))
          return false;
      for(int j = 0; j < g.edges; ++j)
        if(!inL[j] && closes(pi, pv, j))
          del[j] = 1;
      return components(g.iu, g.iv, pi) + components(g.vu, g.vv, pv) == 2;
    }

    void check()
    {
      if((++ticks & 255) == 0 && p.cancel
          && p.cancel->load(std::memory_order_relaxed))
        throw boost::mrt_cancelled();
    }

    void set(int depth, bool out)
    {
      const std::uint64_t bit = std::uint64_t(1) << (63 - depth % 64);
      if(out)
        position[depth / 64] |= bit;
      else
        position[depth / 64] &= ~bit;
    }
    void clear_from(int depth)
    {
      // the bits below `depth` stay 0 (a tree's position ends with zeros)
      for(std::size_t w = depth / 64; w < position.size(); ++w) {
        if(w == std::size_t(depth / 64)) {
          const int b = depth % 64;
          position[w] &= b ? ~((~std::uint64_t(0)) >> b) : 0;
        } else {
          position[w] = 0;
        }
      }
    }

    // the search below this point; `weight` is its share of the whole
    void rec(double weight, int depth)
    {
      check();
      const int left = g.vertices - 1 - in;
      if(left <= 0) {
        if(left == 0) {
          for(int j = 0; j < g.edges; ++j)
            tree[j] = inL[j] != 0;
          clear_from(depth);
          sink->tree(tree, position);
          ++p.found;
        }
        p.done += weight;
        p.advanced();
        return;
      }

      int* pi = di.data() + std::size_t(depth + 1) * g.vertices;
      int* pv = dv.data() + std::size_t(depth + 1) * g.vertices;
      int* mine = list.data() + std::size_t(depth + 1) * (2 * g.edges + 1);
      build(pi, g.iu, g.iv);
      build(pv, g.vu, g.vv);

      // the first edge that can join the tree
      int m = -1;
      for(int j = 0; j < g.edges && m < 0; ++j)
        if(!inL[j] && !del[j] && !closes(pi, pv, j))
          m = j;
      if(m < 0) {
        p.done += weight;
        p.advanced();
        return;
      }

      double used = 0.;

      // m in: the edges closing a cycle with it can no longer be
      inL[m] = 1;
      ++in;
      join(pi, pv, m);
      int gone = 0;
      for(int j = 0; j < g.edges; ++j)
        if(!inL[j] && !del[j] && closes(pi, pv, j)) {
          del[j] = 1;
          mine[gone++] = j;
        }
      set(depth, false);
      used += weight / 2.;
      child(weight / 2., depth + 1);
      for(int k = 0; k < gone; ++k)
        del[mine[k]] = 0;
      inL[m] = 0;
      --in;

      // m out: the bridges left must all be in, without a cycle
      del[m] = 1;
      bridgeList.clear();
      bridges(g.iu, g.iv);
      bridges(g.vu, g.vv);
      build(pi, g.iu, g.iv);
      build(pv, g.vu, g.vv);
      int added = 0;
      bool dead = false;
      for(int e : bridgeList) {
        if(inL[e])
          continue;
        if(!join(pi, pv, e)) {
          dead = true;
          break;
        }
        inL[e] = 1;
        ++in;
        mine[added++] = e;
      }
      if(!dead) {
        set(depth, true);
        used += weight / 2.;
        child(weight / 2., depth + 1);
      }
      for(int k = 0; k < added; ++k)
        inL[mine[k]] = 0;
      in -= added;
      del[m] = 0;

      p.done += weight - used;
      p.advanced();
    }

    // one random path down from here (the state is spoilt): the number
    // of trees it stands for
    double probe(std::uint64_t& seed)
    {
      double weight = 1.;
      for(int depth = 0; ; ++depth) {
        const int left = g.vertices - 1 - in;
        if(left < 0)
          return 0.;
        if(left == 0)
          return weight;
        int* pi = di.data() + std::size_t(depth + 1) * g.vertices;
        int* pv = dv.data() + std::size_t(depth + 1) * g.vertices;
        build(pi, g.iu, g.iv);
        build(pv, g.vu, g.vv);
        int m = -1;
        for(int j = 0; j < g.edges && m < 0; ++j)
          if(!inL[j] && !del[j] && !closes(pi, pv, j))
            m = j;
        if(m < 0)
          return 0.;
        // can m be left out? (its bridges must go in without a cycle)
        del[m] = 1;
        bridgeList.clear();
        bridges(g.iu, g.iv);
        bridges(g.vu, g.vv);
        int* qi = di.data();   // the start's slot: free now
        int* qv = dv.data();
        std::copy(pi, pi + g.vertices, qi);
        std::copy(pv, pv + g.vertices, qv);
        bool dead = false;
        added.clear();
        for(int e : bridgeList) {
          if(inL[e] || std::find(added.begin(), added.end(), e) != added.end())
            continue;
          if(!join(qi, qv, e)) {
            dead = true;
            break;
          }
          added.push_back(e);
        }
        del[m] = 0;
        // xorshift: a random bit
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        const bool out = !dead && (seed & 1);
        if(!dead)
          weight *= 2.;
        if(!out) {
          inL[m] = 1;
          ++in;
          join(pi, pv, m);
          for(int j = 0; j < g.edges; ++j)
            if(!inL[j] && !del[j] && closes(pi, pv, j))
              del[j] = 1;
        } else {
          del[m] = 1;
          for(int e : added) {
            inL[e] = 1;
            ++in;
          }
        }
      }
    }
    std::vector<int> added;

    // on this thread, or another if one is idle and enough is left
    void child(double weight, int depth)
    {
      boost::mrt_pool* pool = p.pool;
      if(pool && p.total && s.group_ && g.vertices - 1 - in >= s.share_
          && pool->hungry())
      {
        std::shared_ptr<walker> w(new walker(s, nullptr));
        w->p = p;
        w->p.done = 0.;
        w->p.found = 0;
        w->p.ticks = 0;
        w->sink.reset(new Sink(sink->part()));
        w->inL = inL;
        w->del = del;
        w->in = in;
        w->position = position;
        tree_search* search = &s;
        pool->submit(*s.group_, [w, weight, depth, search]() {
            w->rec(weight, depth);
            w->p.flush();
            search->merge(*w->sink);
          });
        return;
      }
      rec(weight, depth);
    }
  };

  const tree_graphs& g_;
  std::size_t words_ = 1;
  int share_ = 4;   // hand on parts with at least this many edges left
  boost::mrt_progress* progress_ = nullptr;
  boost::mrt_pool::group* group_ = nullptr;
  std::mutex mutex_;
  std::unique_ptr<Sink> merged_;

};


}


#endif // TREESEARCH_H
