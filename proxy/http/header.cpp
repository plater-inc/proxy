#include "header.hpp"
#include "proxy/util/misc_strings.hpp"
#include <iostream>

namespace proxy {
namespace http {

header_container::iterator
header_container::push_back(const value_type &value) {
  std::string lowercase_name = util::misc_strings::to_lowercase(value.name);
  int new_idx = storage_.size();
  int prev_by_name_idx = -1;
  int next_by_name_idx = -1;
  int prev_by_seq_idx = -1;
  int next_by_seq_idx = -1;
  std::pair<storage_by_name::iterator, bool> header_by_name =
      storage_by_name_.insert(
          {lowercase_name,
           {.first_by_name_idx = new_idx, .last_by_name_idx = new_idx}});
  if (!header_by_name.second) {
    prev_by_name_idx = header_by_name.first->second.last_by_name_idx;
    header_by_name.first->second.last_by_name_idx = new_idx;
    storage_[prev_by_name_idx].next_by_name_idx = new_idx;
  }
  if (first_by_seq_idx == -1) {
    first_by_seq_idx = new_idx;
    last_by_seq_idx = new_idx;
  } else {
    storage_[last_by_seq_idx].next_by_seq_idx = new_idx;
    prev_by_seq_idx = last_by_seq_idx;
  }
  last_by_seq_idx = new_idx;
  storage_.push_back({.lowercase_name = lowercase_name,
                      .prev_by_name_idx = prev_by_name_idx,
                      .next_by_name_idx = next_by_name_idx,
                      .prev_by_seq_idx = prev_by_seq_idx,
                      .next_by_seq_idx = next_by_seq_idx,
                      .header = value});
  return iterator(storage_, new_idx);
}

header_container::name_iterator
header_container::find(std::string lowercase_name) {
  int first_by_name_idx = -1;
  storage_by_name::iterator it = storage_by_name_.find(lowercase_name);
  if (it != storage_by_name_.end()) {
    first_by_name_idx = it->second.first_by_name_idx;
  }
  return name_iterator(storage_, first_by_name_idx);
}

void header_container::erase_all(std::string lowercase_name) {
  storage_by_name::iterator it = storage_by_name_.find(lowercase_name);
  if (it != storage_by_name_.end()) {
    int idx = it->second.first_by_name_idx;
    storage_by_name_.erase(it);
    while (idx != -1) {
      // We make an abandoned element. We dont have to care about
      // storage_by_name_ as we have cleared it already.
      int prev_by_seq_idx = storage_[idx].prev_by_seq_idx;
      int next_by_seq_idx = storage_[idx].next_by_seq_idx;
      if (prev_by_seq_idx == -1) {
        first_by_seq_idx = next_by_seq_idx;
      } else {
        storage_[prev_by_seq_idx].next_by_seq_idx = next_by_seq_idx;
      }
      if (next_by_seq_idx == -1) {
        last_by_seq_idx = prev_by_seq_idx;
      } else {
        storage_[next_by_seq_idx].prev_by_seq_idx = prev_by_seq_idx;
      }
      storage_[idx].lowercase_name.clear();
      storage_[idx].header.value.clear();
      idx = storage_[idx].next_by_name_idx;
    }
  }
}

void header_container::clear() {
  storage_.clear();
  storage_by_name_.clear();
  first_by_seq_idx = -1;
  last_by_seq_idx = -1;
}

#if !defined(NDEBUG)

void header_container::debug_print() {
  std::cout << "header_container:" << std::endl;

  std::cout << "{first_by_seq_idx = " << first_by_seq_idx
            << ", last_by_seq_idx = " << last_by_seq_idx << "}" << std::endl;

  std::cout << "storage_:" << std::endl;
  for (int idx = 0; idx < storage_.size(); idx++) {
    std::cout << idx << ": {lowercase_name = " << storage_[idx].lowercase_name
              << ", prev_by_name_idx = " << storage_[idx].prev_by_name_idx
              << ", next_by_name_idx = " << storage_[idx].next_by_name_idx
              << ", prev_by_seq_idx = " << storage_[idx].prev_by_seq_idx
              << ", next_by_seq_idx = " << storage_[idx].next_by_seq_idx
              << ", name = " << storage_[idx].header.name
              << ", value = " << storage_[idx].header.value << "}" << std::endl;
  }

  std::cout << "storage_by_name_:" << std::endl;
  for (storage_by_name::iterator it = storage_by_name_.begin();
       it != storage_by_name_.end(); it++) {
    std::cout << it->first
              << ": {first_by_name_idx = " << it->second.first_by_name_idx
              << ", last_by_name_idx = " << it->second.last_by_name_idx << "}"
              << std::endl;
  }
}

#endif

} // namespace http
} // namespace proxy