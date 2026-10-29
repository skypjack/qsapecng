/*
    QSapecNG - Qt based SapecNG GUI front-end
    Copyright (C) 2009, Michele Caini

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


#ifndef METACIRCUIT_THREAD_H
#define METACIRCUIT_THREAD_H


#include "model/metacircuit.h"
#include "parser/parser.h"

#include <QMutexLocker>
#include <QThread>
#include <QMutex>

#include "boost-sapecng/mrt_pool.hpp"
#include "boost-sapecng/mrt_progress.hpp"

#include <algorithm>
#include <chrono>
#include <atomic>
#include <memory>
#include <string>
#include <vector>


namespace qsapecng
{


class MetaCircuit_Thread: public QThread
{

  Q_OBJECT

public:
  MetaCircuit_Thread(QObject* parent = 0): QThread(parent) { }

  // A Resolve runs in two phases:
  //   Quick: the trees are counted (or estimated), then the numeric result
  //     (digit) is worked out for every output: a second at most. When the
  //     symbolic result is quick to get too (the trees were counted in a
  //     moment), it is done right away, unless numeric only is asked for.
  //   Symbolic: the whole result (raw, mixed and digit), listing the
  //     trees: can take hours for a big circuit.
  enum Phase { Quick, Symbolic };
  inline void setPhase(Phase phase) { phase_ = phase; }
  inline Phase phase() const { return phase_; }
  // Quick phase: never the symbolic result
  inline void setNumericOnly(bool numeric) { numericOnly_ = numeric; }
  // after a Quick phase: the result is the whole one (raw, mixed too)
  inline bool symbolicDone() const { return symbolicDone_; }
  // the trees (terms) the symbolic result adds up, for all the outputs;
  // exact when they were counted
  inline double estimatedTrees() const { return trees_; }
  inline bool treesCounted() const { return counted_; }
  // the cores used (of the most allowed)
  inline int threads() const { return maxThreads_; }

  // the most cores (threads) a solve uses, for all its outputs together
  inline void setMaxThreads(int n) { maxThreads_ = std::max(1, n); }
  // the most distinct terms a symbolic result may keep (the memory it
  // takes); 0: no limit. Over it the symbolic phase stops, tooLarge()
  inline void setTermLimit(long long n) { termLimit_ = std::max(0LL, n); }
  inline bool tooLarge() const { return tooLarge_.load(); }
  inline long long tooLargeTerms() const { return tooLargeTerms_.load(); }

  // Symbolic phase, while it runs: the trees found so far and the time
  // since it started (the progress bar and the time left come from them)
  inline long long treesFound() const { return found_.load(); }
  inline double elapsed() const
    {
      return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count()
        - started_.load();
    }

  // asks a running solve to stop; it ends soon after, cancelled()
  inline void cancel() { cancel_.store(true); }
  inline bool cancelled() const { return cancelled_.load(); }

  // 0 - 1000 over all the outputs, and how many are done; safe while
  // running. With an estimate of the trees: the trees found so far of
  // those (at most 99.9 % until it is done)
  inline int progress() const
    {
      if(phase_ == Symbolic && trees_ > 0.)
        return int(std::min(999., 1000. * double(found_.load()) / trees_));
      if(permille_.empty())
        return 0;
      long sum = 0;
      for(const auto& p : permille_)
        sum += std::min(1000, p->load(std::memory_order_relaxed));
      return int(sum / long(permille_.size()));
    }
  inline int outputsDone() const { return done_.load(); }
  inline int outputTotal() const { return int(permille_.size()); }

  // builds one circuit per output of the netlist (Out markers, voltmeters,
  // ammeters): the parser is run once per output
  inline void apply(sapecng::abstract_parser& parser)
    {
      QMutexLocker locker(&mutex_);
      outputs_.clear();
      unsigned int count = 1;
      for(unsigned int k = 0; k < count; ++k) {
        Output out;
        out.circuit.reset(new sapecng::circuit);
        out.meta.reset(new sapecng::metacircuit);
        out.meta->keep_terms(true);   // for the next solve: see cache_
        sapecng::circuit_builder builder(*out.circuit, k);
        parser.parse(builder);
        if(k == 0)
          count = std::max<std::size_t>(1, out.circuit->outputNames().size());
        if(k < out.circuit->outputNames().size())
          out.name = out.circuit->outputNames().at(k);
        outputs_.push_back(std::move(out));
      }
      permille_.clear();
      for(std::size_t k = 0; k < outputs_.size(); ++k)
        permille_.emplace_back(new std::atomic<int>(0));
      done_.store(0);
    }

  inline int outputCount()
    { QMutexLocker locker(&mutex_); return int(outputs_.size()); }
  inline std::string outputName(int k)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].name : std::string(); }

  inline sapecng::metacircuit::expression raw_numerator(int k = 0)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].meta->raw().first : empty(); }
  inline sapecng::metacircuit::expression raw_denominator(int k = 0)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].meta->raw().second : empty(); }
  inline sapecng::metacircuit::expression digit_numerator(int k = 0)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].meta->digit().first : empty(); }
  inline sapecng::metacircuit::expression digit_denominator(int k = 0)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].meta->digit().second : empty(); }
  inline sapecng::metacircuit::expression mixed_numerator(int k = 0)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].meta->mixed().first : empty(); }
  inline sapecng::metacircuit::expression mixed_denominator(int k = 0)
    { QMutexLocker locker(&mutex_); return valid(k) ? outputs_[k].meta->mixed().second : empty(); }
  inline bool singular(int k = 0)
    { QMutexLocker locker(&mutex_); return !valid(k) || outputs_[k].meta->singular(); }

protected:
  // all the outputs share one pool of maxThreads_ threads: several outputs
  // are solved at the same time, and a single big one is split among them
  void run()
  {
    QMutexLocker locker(&mutex_);
    cancel_.store(false);
    cancelled_.store(false);
    tooLarge_.store(false);
    tooLargeTerms_.store(0);
    done_.store(0);
    found_.store(0);
    started_.store(std::chrono::duration<double>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
    for(auto& p : permille_)
      p->store(0);

    boost::mrt_pool pool{ unsigned(maxThreads_) };

    if(phase_ == Quick) {
      symbolicDone_ = false;
      // the trees of every output: counted, or estimated
      trees_ = 0.;
      counted_ = true;
      for(std::size_t k = 0; k < outputs_.size(); ++k) {
        Output& out = outputs_[k];
        if(cancel_.load())
          break;
        // only values changed since its last symbolic result: it takes
        // no time. Otherwise the terms kept are of no more use
        if(k < cache_.size() && cache_[k]) {
          if(sapecng::metacircuit::fits(*out.circuit, *cache_[k])) {
            out.trees = 0.;
            continue;
          }
          cache_[k].reset();
        }
        boost::mrt_progress progress;
        progress.cancel = &cancel_;
        progress.pool = &pool;
        boost::mrt_progress_scope scope(&progress);
        bool exact = true;
        try {
          out.trees = sapecng::metacircuit::estimate_trees(*out.circuit,
            0.3, &exact);
        } catch(...) {
          out.trees = 0.;
        }
        trees_ += out.trees;
        counted_ = counted_ && exact;
      }
      if(cancel_.load()) {
        cancelled_.store(true);
        return;
      }
      // quick to get: the whole result now
      if(counted_ && !numericOnly_) {
        symbolicDone_ = true;
        solveAll(pool, false);
        keepTerms();
        return;
      }
      solveAll(pool, true);
      return;
    }

    solveAll(pool, false);
    keepTerms();
  }

private:
  void solveAll(boost::mrt_pool& pool, bool numeric)
  {
    boost::mrt_pool::group all;
    for(std::size_t k = 0; k < outputs_.size(); ++k)
      pool.submit_root(all, [this, k, &pool, numeric]() {
          solve(k, pool, numeric);
        });
    pool.wait_all(all);   // runs outputs too; their failures are theirs
  }

  void solve(std::size_t k, boost::mrt_pool& pool, bool numeric)
  {
    std::atomic<long long> total(0);
    boost::mrt_progress progress;
    progress.cancel = &cancel_;
    progress.permille = permille_[k].get();
    progress.pool = &pool;
    progress.total = &total;
    progress.trees = &found_;
    progress.term_limit = termLimit_;
    try {
      boost::mrt_progress_scope scope(&progress);
      if(numeric)
        outputs_[k].meta->numeric(*outputs_[k].circuit);
      else if(k >= cache_.size()
          || !outputs_[k].meta->reuse(*outputs_[k].circuit, cache_[k]))
        (*outputs_[k].meta)(*outputs_[k].circuit);
    } catch(const sapecng::metacircuit::too_large& e) {
      // more terms than the memory holds: the other outputs stop too
      tooLargeTerms_.store(e.terms);
      tooLarge_.store(true);
      cancelled_.store(true);
      cancel_.store(true);
    } catch(const boost::mrt_cancelled&) {
      cancelled_.store(true);
      cancel_.store(true);   // the other outputs stop too
    } catch(...) {
      // out of memory and the like: this output stays without a result
    }
    permille_[k]->store(1000);
    done_.fetch_add(1);
  }

  // the terms of each output's symbolic result, for the next solve
  void keepTerms()
  {
    cache_.resize(outputs_.size());
    for(std::size_t k = 0; k < outputs_.size(); ++k)
      if(outputs_[k].meta->terms())
        cache_[k] = outputs_[k].meta->terms();
  }

private:
  struct Output
  {
    std::string name;
    std::unique_ptr<sapecng::circuit> circuit;
    std::unique_ptr<sapecng::metacircuit> meta;
    double trees = 0.;
  };

  inline bool valid(int k) const
    { return k >= 0 && k < int(outputs_.size()); }
  static sapecng::metacircuit::expression empty()
    { return sapecng::metacircuit::expression(); }

  std::vector<Output> outputs_;
  // Each output's symbolic terms (raw) from the last solve that had them.
  // A new value of a part (or a part made symbolic or not) leaves them the
  // same: the symbolic result is then made from them at once, instead of
  // listing the trees again (see metacircuit::reuse)
  std::vector< std::shared_ptr<const sapecng::metacircuit::term_cache> >
    cache_;
  std::vector< std::unique_ptr< std::atomic<int> > > permille_;
  std::atomic<bool> cancel_{false};
  std::atomic<bool> cancelled_{false};
  std::atomic<int> done_{0};
  std::atomic<long long> found_{0};
  std::atomic<bool> tooLarge_{false};
  std::atomic<long long> tooLargeTerms_{0};
  long long termLimit_ = 0;
  std::atomic<double> started_{0.};   // steady clock, seconds
  Phase phase_ = Quick;
  bool numericOnly_ = false;
  bool symbolicDone_ = false;
  double trees_ = 0.;
  bool counted_ = false;
  int maxThreads_ = 1;
  QMutex mutex_;

};


}


#endif // METACIRCUIT_THREAD_H
