/*
    SapecNG - Next Generation Symbolic Analysis Program for Electric Circuit
    Copyright (C) 2026, QSapecNG contributors

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


#ifndef SYMBOLS_H
#define SYMBOLS_H


#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>


namespace sapecng
{


// The names of a term (R1 C2 L3): what std::list<std::string> was, small.
// A big circuit's symbolic result has millions of terms of a dozen names
// or more each; as lists of strings that took over 1 KB a term, twice
// (Raw and Result) and again for every copy. Here each name is kept once
// for the whole program (interned) and a term holds pointers to them, in
// one array shared by its copies until one of them is changed.
class symbol_list
{

  typedef std::vector<const std::string*> names;

public:
  // the one copy of a name
  static const std::string* intern(const std::string& name)
  {
    static std::mutex mutex;
    static std::unordered_set<std::string> all;   // nodes do not move
    std::lock_guard<std::mutex> lock(mutex);
    return &*all.insert(name).first;
  }

  class const_iterator
  {
  public:
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef std::string value_type;
    typedef std::ptrdiff_t difference_type;
    typedef const std::string* pointer;
    typedef const std::string& reference;

    const_iterator() { }
    explicit const_iterator(names::const_iterator it): it_(it) { }
    reference operator*() const { return **it_; }
    pointer operator->() const { return *it_; }
    const_iterator& operator++() { ++it_; return *this; }
    const_iterator operator++(int) { const_iterator t(*this); ++it_; return t; }
    const_iterator& operator--() { --it_; return *this; }
    const_iterator operator--(int) { const_iterator t(*this); --it_; return t; }
    bool operator==(const const_iterator& o) const { return it_ == o.it_; }
    bool operator!=(const const_iterator& o) const { return it_ != o.it_; }
    names::const_iterator base() const { return it_; }
  private:
    names::const_iterator it_;
  };
  typedef const_iterator iterator;   // the names cannot be changed in place
  typedef std::string value_type;
  typedef std::size_t size_type;

  symbol_list() { }
  symbol_list(std::initializer_list<std::string> list)
    { for(const std::string& s : list) push_back(s); }

  const_iterator begin() const
    { return const_iterator(p_ ? p_->begin() : empty_names().begin()); }
  const_iterator end() const
    { return const_iterator(p_ ? p_->end() : empty_names().end()); }
  size_type size() const { return p_ ? p_->size() : 0; }
  bool empty() const { return size() == 0; }
  const std::string& front() const { return *p_->front(); }
  const std::string& back() const { return *p_->back(); }

  void push_back(const std::string& name) { own().push_back(intern(name)); }
  // a name interned already (intern()): no look-up
  void push_back_interned(const std::string* name) { own().push_back(name); }
  void reserve(size_type n) { own().reserve(n); }
  void clear() { p_.reset(); }
  // in alphabetical order (as std::list<std::string>::sort)
  void sort()
  {
    if(size() < 2)
      return;
    names& v = own();
    std::sort(v.begin(), v.end(),
      [](const std::string* a, const std::string* b) { return *a < *b; });
  }
  const_iterator erase(const_iterator at)
  {
    const std::ptrdiff_t i = at.base() - p_->begin();
    names& v = own();
    return const_iterator(v.erase(v.begin() + i));
  }

  bool operator==(const symbol_list& o) const
  {
    if(p_ == o.p_)
      return true;
    if(size() != o.size())
      return false;
    if(empty())
      return true;   // one of them may have no array at all
    // one copy of every name: the same name is the same pointer
    return std::equal(p_->begin(), p_->end(), o.p_->begin());
  }
  bool operator!=(const symbol_list& o) const { return !(*this == o); }
  bool operator<(const symbol_list& o) const
  {
    return std::lexicographical_compare(begin(), end(), o.begin(), o.end());
  }

private:
  // the array to change: this term's own (copied if shared)
  names& own()
  {
    if(!p_)
      p_ = std::make_shared<names>();
    else if(p_.use_count() > 1)
      p_ = std::make_shared<names>(*p_);
    return *p_;
  }
  static const names& empty_names()
  {
    static const names none;
    return none;
  }

  std::shared_ptr<names> p_;

};


}


#endif // SYMBOLS_H
