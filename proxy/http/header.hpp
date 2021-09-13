#ifndef PROXY_HTTP_HEADER_HPP
#define PROXY_HTTP_HEADER_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace proxy {
namespace http {

struct header {
  const std::string name{};
  std::string value{};
};

class header_container {
private:
  struct stored_header {
    std::string lowercase_name{};
    int prev_by_name_idx{};
    int next_by_name_idx{};
    int prev_by_seq_idx{};
    int next_by_seq_idx{};
    header header{};
  };
  typedef std::vector<stored_header> storage;
  storage storage_;
  int first_by_seq_idx{-1};
  int last_by_seq_idx{-1};
  struct header_by_name {
    int first_by_name_idx{};
    int last_by_name_idx{};
  };
  typedef std::unordered_map<std::string, header_by_name> storage_by_name;
  storage_by_name storage_by_name_{};

public:
  typedef header value_type;

  struct name_iterator;
  struct iterator {
    typedef iterator self_type;
    typedef header value_type;
    typedef header &reference;
    typedef header *pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef int difference_type;
    iterator(storage &storage, int idx) : storage_(storage), idx_(idx) {}
    self_type operator++() {
      self_type i = *this;
      idx_ = storage_[idx_].next_by_seq_idx;
      return i;
    }
    self_type operator++(int) {
      idx_ = storage_[idx_].next_by_seq_idx;
      return *this;
    }
    reference operator*() { return storage_[idx_].header; }
    pointer operator->() { return &storage_[idx_].header; }
    bool operator==(const self_type &rhs) const { return idx_ == rhs.idx_; }
    bool operator!=(const self_type &rhs) const { return idx_ != rhs.idx_; }
    bool operator==(const name_iterator &rhs) const { return idx_ == rhs.idx_; }
    bool operator!=(const name_iterator &rhs) const { return idx_ != rhs.idx_; }
    friend struct name_iterator;

  private:
    storage &storage_;
    int idx_;
  };

  struct name_iterator {
    typedef name_iterator self_type;
    typedef header value_type;
    typedef header &reference;
    typedef header *pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef int difference_type;
    name_iterator(storage &storage, int idx) : storage_(storage), idx_(idx) {}
    self_type operator++() {
      self_type i = *this;
      idx_ = storage_[idx_].next_by_name_idx;
      return i;
    }
    self_type operator++(int) {
      idx_ = storage_[idx_].next_by_name_idx;
      return *this;
    }
    reference operator*() { return storage_[idx_].header; }
    pointer operator->() { return &storage_[idx_].header; }
    bool operator==(const self_type &rhs) const { return idx_ == rhs.idx_; }
    bool operator!=(const self_type &rhs) const { return idx_ != rhs.idx_; }
    bool operator==(const iterator &rhs) const { return idx_ == rhs.idx_; }
    bool operator!=(const iterator &rhs) const { return idx_ != rhs.idx_; }

  private:
    storage &storage_;
    int idx_;
    friend struct iterator;
  };

  iterator push_back(const value_type &value);

  iterator begin() { return iterator(storage_, first_by_seq_idx); }
  iterator end() { return iterator(storage_, -1); }

  name_iterator find(std::string lowercase_name);
  void erase_all(std::string lowercase_name);
  bool empty() { return first_by_seq_idx == -1; }
  void clear();

#if !defined(NDEBUG)
  void debug_print();
#endif
};

} // namespace http
} // namespace proxy

#endif // PROXY_HTTP_HEADER_HPP
