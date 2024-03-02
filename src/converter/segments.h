// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_CONVERTER_SEGMENTS_H_
#define MOZC_CONVERTER_SEGMENTS_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/container/freelist.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/strings/assign.h"
#include "converter/lattice.h"
#include "testing/friend_test.h"

#ifndef NDEBUG
#define MOZC_CANDIDATE_DEBUG
#define MOZC_CANDIDATE_LOG(result, message) \
  (result)->Dlog(__FILE__, __LINE__, message)

#else  // NDEBUG
#define MOZC_CANDIDATE_LOG(result, message) \
  {}

#endif  // NDEBUG

namespace mozc {

class Segment final {
 public:
  enum SegmentType {
    FREE,            // FULL automatic conversion.
    FIXED_BOUNDARY,  // cannot consist of multiple segments.
    FIXED_VALUE,     // cannot consist of multiple segments.
                     // and result is also fixed
    SUBMITTED,       // submitted node
    HISTORY          // history node. It is hidden from user.
  };

  struct Candidate {
    // LINT.IfChange
    enum Attribute {
      DEFAULT_ATTRIBUTE = 0,
      // this was the best candidate before learning
      BEST_CANDIDATE = 1 << 0,
      // this candidate was reranked by user
      RERANKED = 1 << 1,
      // don't save it in history
      NO_HISTORY_LEARNING = 1 << 2,
      // don't save it in suggestion
      NO_SUGGEST_LEARNING = 1 << 3,
      // NO_HISTORY_LEARNING | NO_SUGGEST_LEARNING
      NO_LEARNING = (1 << 2 | 1 << 3),
      // learn it with left/right context
      CONTEXT_SENSITIVE = 1 << 4,
      // has "did you mean"
      SPELLING_CORRECTION = 1 << 5,
      // No need to have full/half width expansion
      NO_VARIANTS_EXPANSION = 1 << 6,
      // No need to have extra descriptions
      NO_EXTRA_DESCRIPTION = 1 << 7,
      // was generated by real-time conversion
      REALTIME_CONVERSION = 1 << 8,
      // contains tokens in user dictionary.
      USER_DICTIONARY = 1 << 9,
      // command candidate. e.g., incognito mode.
      COMMAND_CANDIDATE = 1 << 10,
      // key characters are consumed partially.
      // Consumed size is |consumed_key_size|.
      // If not set, all the key characters are consumed.
      PARTIALLY_KEY_CONSUMED = 1 << 11,
      // Typing correction candidate.
      // - Special description should be shown when the candidate is created
      //   by a dictionary predictor.
      // - No description should be shown when the candidate is loaded from
      //   history.
      // - Otherwise following unexpected behavior can be observed.
      //   1. Type "やんしょん" and submit "マンション" (annotated with "補正").
      //   2. Type "まんしょん".
      //   3. "マンション" (annotated with "補正") is shown as a candidate
      //      regardless of a user's correct typing.
      TYPING_CORRECTION = 1 << 12,
      // Auto partial suggestion candidate.
      // - Special description should be shown when the candidate is created
      //   by a dictionary predictor.
      // - No description should be shown when the candidate is loaded from
      //   history.
      AUTO_PARTIAL_SUGGESTION = 1 << 13,
      // Predicted from user prediction history.
      USER_HISTORY_PREDICTION = 1 << 14,
      // Contains suffix dictionary.
      SUFFIX_DICTIONARY = 1 << 15,
      // Disables modification and removal in rewriters.
      NO_MODIFICATION = 1 << 16,
    };
    // LINT.ThenChange(//converter/converter_main.cc)

    enum Command {
      DEFAULT_COMMAND = 0,
      ENABLE_INCOGNITO_MODE,      // enables "incognito mode".
      DISABLE_INCOGNITO_MODE,     // disables "incognito mode".
      ENABLE_PRESENTATION_MODE,   // enables "presentation mode".
      DISABLE_PRESENTATION_MODE,  // disables "presentation mode".
    };

    // Bit field indicating candidate source info.
    // This should be used for usage stats.
    // TODO(mozc-team): Move Attribute fields for source info
    // to SourceInfo.
    enum SourceInfo {
      SOURCE_INFO_NONE = 0,
      // Attributes for zero query suggestion.
      // These are used for usage stats.
      // For DICTIONARY_PREDICTOR_ZERO_QUERY_XX, XX stands for the
      // types defined at zero_query_list.h.
      DICTIONARY_PREDICTOR_ZERO_QUERY_NONE = 1 << 0,
      DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX = 1 << 1,
      DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON = 1 << 2,
      DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI = 1 << 3,
      DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM = 1 << 4,
      DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX = 1 << 5,

      USER_HISTORY_PREDICTOR = 1 << 6,
    };

    enum Category {
      DEFAULT_CATEGORY,  // Realtime conversion, history prediction, etc
      SYMBOL,            // Symbol, emoji
      OTHER,             // Misc candidate
    };

    // LINT.IfChange
    std::string key;    // reading
    std::string value;  // surface form
    std::string content_key;
    std::string content_value;

    size_t consumed_key_size = 0;

    // Meta information
    std::string prefix;
    std::string suffix;
    // Description including description type and message
    std::string description;
    // Description for A11y support (e.g. "あ。ヒラガナ あ")
    std::string a11y_description;

    // Usage ID
    int32_t usage_id = 0;
    // Title of the usage containing basic form of this candidate.
    std::string usage_title;
    // Content of the usage.
    std::string usage_description;

    // Context "sensitive" candidate cost.
    // Taking adjacent words/nodes into consideration.
    // Basically, candidate is sorted by this cost.
    int32_t cost = 0;
    // Context "free" candidate cost
    // NOT taking adjacent words/nodes into consideration.
    int32_t wcost = 0;
    // (cost without transition cost between left/right boundaries)
    // Cost of only transitions (cost without word cost adjacent context)
    int32_t structure_cost = 0;

    // lid of left-most node
    uint16_t lid = 0;
    // rid of right-most node
    uint16_t rid = 0;

    // Attributes of this candidate. Can set multiple attributes
    // defined in enum |Attribute|.
    uint32_t attributes = 0;

    // Candidate's source info which will be used for usage stats.
    uint32_t source_info = SOURCE_INFO_NONE;

    Category category = DEFAULT_CATEGORY;

    // Candidate style. This is not a bit-field.
    // The style is defined in enum |Style|.
    NumberUtil::NumberString::Style style =
        NumberUtil::NumberString::DEFAULT_STYLE;

    // Command of this candidate. This is not a bit-field.
    // The style is defined in enum |Command|.
    Command command = DEFAULT_COMMAND;

    // Boundary information for real time conversion.  This will be set only for
    // real time conversion result candidates.  Each element is the encoded
    // lengths of key, value, content key and content value.
    std::vector<uint32_t> inner_segment_boundary;
    // LINT.ThenChange(//converter/segments_matchers.h)

    // The original cost before rescoring. Used for debugging purpose.
    int32_t cost_before_rescoring = 0;
#ifdef MOZC_CANDIDATE_DEBUG
    void Dlog(absl::string_view filename, int line,
              absl::string_view message) const;
    mutable std::string log;
#endif  // MOZC_CANDIDATE_DEBUG

    static bool EncodeLengths(size_t key_len, size_t value_len,
                              size_t content_key_len, size_t content_value_len,
                              uint32_t *result);

    // This function ignores error, so be careful when using this.
    static uint32_t EncodeLengths(size_t key_len, size_t value_len,
                                  size_t content_key_len,
                                  size_t content_value_len) {
      uint32_t result;
      EncodeLengths(key_len, value_len, content_key_len, content_value_len,
                    &result);
      return result;
    }

    // Inserts a new element to |inner_segment_boundary|.  If one of four
    // lengths is longer than 255, this method returns false.
    bool PushBackInnerSegmentBoundary(size_t key_len, size_t value_len,
                                      size_t content_key_len,
                                      size_t content_value_len);

    // Iterates inner segments.  Usage example:
    // for (InnerSegmentIterator iter(&cand); !iter.Done(); iter.Next()) {
    //   absl::string_view s = iter.GetContentKey();
    //   ...
    // }
    class InnerSegmentIterator final {
     public:
      explicit InnerSegmentIterator(const Candidate *candidate)
          : candidate_(candidate),
            key_offset_(candidate->key.data()),
            value_offset_(candidate->value.data()),
            index_(0) {}

      bool Done() const {
        return index_ == candidate_->inner_segment_boundary.size();
      }

      void Next();
      absl::string_view GetKey() const;
      absl::string_view GetValue() const;
      absl::string_view GetContentKey() const;
      absl::string_view GetContentValue() const;
      absl::string_view GetFunctionalKey() const;
      absl::string_view GetFunctionalValue() const;
      size_t GetIndex() const { return index_; }

     private:
      const Candidate *candidate_;
      const char *key_offset_;
      const char *value_offset_;
      size_t index_;
    };

    // Clears the Candidate with default values. Note that the default
    // constructor already does the same so you don't need to call Clear
    // explicitly.
    void Clear();

    // Returns functional key.
    // functional_key =
    // key.substr(content_key.size(), key.size() - content_key.size());
    absl::string_view functional_key() const;

    // Returns functional value.
    // functional_value =
    // value.substr(content_value.size(), value.size() - content_value.size());
    absl::string_view functional_value() const;

    // Returns whether the inner_segment_boundary member is consistent with
    // key and value.
    // Note: content_key and content_value are not checked here.
    // We cannot compose candidate's content_key and content_value directly
    // from the inner segments in the current implementation.
    // Example:
    // value: 車のほうがあとだ
    // content_value: 車のほうがあとだ
    // inner_segments:
    // <くるまのほうが, 車のほうが, くるま, 車>
    // <あとだ, あとだ, あとだ, あとだ>
    bool IsValid() const;
    std::string DebugString() const;

    friend std::ostream &operator<<(std::ostream &os,
                                    const Candidate &candidate) {
      return os << candidate.DebugString();
    }
  };

  Segment() : segment_type_(FREE), pool_(kCandidatesPoolSize) {}

  Segment(const Segment &x);
  Segment &operator=(const Segment &x);

  SegmentType segment_type() const { return segment_type_; }
  void set_segment_type(const SegmentType segment_type) {
    segment_type_ = segment_type;
  }

  const std::string &key() const { return key_; }
  template <typename T>
  void set_key(T &&key) {
    strings::Assign(key_, std::forward<T>(key));
  }

  // check if the specified index is valid or not.
  bool is_valid_index(int i) const;

  // Candidate manipulations
  // getter
  const Candidate &candidate(int i) const;

  // setter
  Candidate *mutable_candidate(int i);

  // push and insert candidates
  Candidate *push_front_candidate();
  Candidate *push_back_candidate();
  // alias of push_back_candidate()
  Candidate *add_candidate() { return push_back_candidate(); }
  Candidate *insert_candidate(int i);
  void insert_candidate(int i, std::unique_ptr<Candidate> candidate);
  void insert_candidates(int i,
                         std::vector<std::unique_ptr<Candidate>> candidates);

  // get size of candidates
  size_t candidates_size() const { return candidates_.size(); }

  // erase candidate
  void pop_front_candidate();
  void pop_back_candidate();
  void erase_candidate(int i);
  void erase_candidates(int i, size_t size);

  // erase all candidates
  // do not erase meta candidates
  void clear_candidates();

  // meta candidates
  // TODO(toshiyuki): Integrate meta candidates to candidate and delete these
  size_t meta_candidates_size() const { return meta_candidates_.size(); }
  void clear_meta_candidates() { meta_candidates_.clear(); }
  const std::vector<Candidate> &meta_candidates() const {
    return meta_candidates_;
  }
  std::vector<Candidate> *mutable_meta_candidates() {
    return &meta_candidates_;
  }
  const Candidate &meta_candidate(size_t i) const;
  Candidate *mutable_meta_candidate(size_t i);
  Candidate *add_meta_candidate();

  // move old_idx-th-candidate to new_index
  void move_candidate(int old_idx, int new_idx);

  void Clear();

  // Keep clear() method as other modules are still using the old method
  void clear() { Clear(); }

  std::string DebugString() const;

  friend std::ostream &operator<<(std::ostream &os, const Segment &segment) {
    return os << segment.DebugString();
  }

  const std::deque<Candidate *> &candidates() const { return candidates_; }

  // For debug. Candidate words removed through conversion process.
  std::vector<Candidate> removed_candidates_for_debug_;

 private:
  void DeepCopyCandidates(const std::deque<Candidate *> &candidates);

  static constexpr int kCandidatesPoolSize = 16;

  // LINT.IfChange
  SegmentType segment_type_;
  // Note that |key_| is shorter than usual when partial suggestion is
  // performed.
  // For example if the preedit text is "しれ|ません", there is only a segment
  // whose |key_| is "しれ".
  // There is no way to detect by using only a segment whether this segment is
  // for partial suggestion or not.
  // You should detect that by using both Composer and Segments.
  std::string key_;
  std::deque<Candidate *> candidates_;
  std::vector<Candidate> meta_candidates_;
  std::vector<std::unique_ptr<Candidate>> pool_;
  // LINT.ThenChange(//converter/segments_matchers.h)
};

// Segments is basically an array of Segment.
// Note that there are two types of Segment
// a) History Segment (SegmentType == HISTORY OR SUBMITTED)
//    Segments user entered just before the transacton
// b) Conversion Segment
//    Current segments user inputs
//
// Array of segment is represented as an array as follows
// segments_array[] = {HS_0,HS_1,...HS_N, CS0, CS1, CS2...}
//
// * segment(i) and mutable_segment(int i)
//  access segment regardless of History/Conversion distinctions
//
// * history_segment(i) and mutable_history_segment(i)
//  access only History Segment
//
// conversion_segment(i) and mutable_conversion_segment(i)
//  access only Conversion Segment
//  segment(i + history_segments_size()) == conversion_segment(i)
class Segments final {
 public:
  // Client of segments can remember any string which can be used
  // to revert the last Finish operation.
  // "id" can be used for identifying the purpose of the key;
  struct RevertEntry {
    enum RevertEntryType {
      CREATE_ENTRY,
      UPDATE_ENTRY,
    };
    uint16_t revert_entry_type = 0;
    // UserHitoryPredictor uses '1' for now.
    // Do not use duplicate keys.
    uint16_t id = 0;
    uint32_t timestamp = 0;
    std::string key;
  };

  // This class wraps an iterator as is, except that `operator*` dereferences
  // twice. For example, if `InnnerIterator` is the iterator of
  // `std::deque<Segment *>`, `operator*` dereferences to `Segment&`.
  using inner_iterator = std::deque<Segment *>::iterator;
  using inner_const_iterator = std::deque<Segment *>::const_iterator;
  template <typename InnerIterator, bool is_const = false>
  class Iterator {
   public:
    using inner_value_type =
        typename std::iterator_traits<InnerIterator>::value_type;

    using iterator_category =
        typename std::iterator_traits<InnerIterator>::iterator_category;
    using value_type = std::conditional_t<
        is_const,
        typename std::add_const_t<std::remove_pointer_t<inner_value_type>>,
        typename std::remove_pointer_t<inner_value_type>>;
    using difference_type =
        typename std::iterator_traits<InnerIterator>::difference_type;
    using pointer = value_type *;
    using reference = value_type &;

    explicit Iterator(const InnerIterator &iterator) : iterator_(iterator) {}

    // Make `iterator` type convertible to `const_iterator`.
    template <bool enable = is_const>
    Iterator(typename std::enable_if_t<enable, const Iterator<inner_iterator>>
                 iterator)
        : iterator_(iterator.iterator_) {}

    reference operator*() const { return **iterator_; }
    pointer operator->() const { return *iterator_; }

    Iterator &operator++() {
      ++iterator_;
      return *this;
    }
    Iterator &operator--() {
      --iterator_;
      return *this;
    }
    Iterator operator+(difference_type diff) const {
      return Iterator{iterator_ + diff};
    }
    Iterator operator-(difference_type diff) const {
      return Iterator{iterator_ - diff};
    }
    Iterator &operator+=(difference_type diff) {
      iterator_ += diff;
      return *this;
    }

    difference_type operator-(const Iterator &other) const {
      return iterator_ - other.iterator_;
    }

    bool operator==(const Iterator &other) const {
      return iterator_ == other.iterator_;
    }
    bool operator!=(const Iterator &other) const {
      return iterator_ != other.iterator_;
    }

   private:
    friend class Iterator<inner_const_iterator, /*is_const=*/true>;
    friend class Segments;

    InnerIterator iterator_;
  };
  using iterator = Iterator<inner_iterator>;
  using const_iterator = Iterator<inner_const_iterator, /*is_const=*/true>;
  static_assert(!std::is_const_v<iterator::value_type>);
  static_assert(std::is_const_v<const_iterator::value_type>);

  // This class represents `begin` and `end`, like a `std::span` for
  // non-contiguous iterators.
  template <typename Iterator>
  class Range {
   public:
    using difference_type = typename Iterator::difference_type;
    using reference = typename Iterator::reference;

    Range(const Iterator &begin, const Iterator &end)
        : begin_(begin), end_(end) {}

    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }

    difference_type size() const { return end_ - begin_; }
    bool empty() const { return begin_ == end_; }

    reference operator[](difference_type index) const {
      CHECK_GE(index, 0);
      CHECK_LT(index, size());
      return *(begin_ + index);
    }
    reference front() const {
      CHECK(!empty());
      return *begin_;
    }
    reference back() const {
      CHECK(!empty());
      return *(end_ - 1);
    }

    // Skip first `count` elements, similar to `std::ranges::views::drop`.
    Range drop(difference_type count) const {
      CHECK_GE(count, 0);
      return Range{count < size() ? begin_ + count : end_, end_};
    }
    // Take first `count` elements, similar to `std::ranges::views::take`.
    Range take(difference_type count) const {
      CHECK_GE(count, 0);
      return Range{begin_, count < size() ? begin_ + count : end_};
    }
    // Take `count` segments from the end.
    Range take_last(difference_type count) const {
      CHECK_GE(count, 0);
      return drop(std::max<difference_type>(0, size() - count));
    }
    // Same as `drop(drop_count).take(count)`, providing better readability.
    // `std::ranges::subrange` may not provide the same argument types though.
    Range subrange(difference_type index, difference_type count) const {
      return drop(index).take(count);
    }

   private:
    Iterator begin_;
    Iterator end_;
  };

  // constructors
  Segments()
      : max_history_segments_size_(0),
        resized_(false),
        pool_(32),
        cached_lattice_() {}

  Segments(const Segments &x);
  Segments &operator=(const Segments &x);

  // iterators
  iterator begin() { return iterator{segments_.begin()}; }
  iterator end() { return iterator{segments_.end()}; }
  const_iterator begin() const { return const_iterator{segments_.begin()}; }
  const_iterator end() const { return const_iterator{segments_.end()}; }

  // ranges
  template <typename Iterator>
  static Range<Iterator> make_range(const Iterator &begin,
                                    const Iterator &end) {
    return Range<Iterator>(begin, end);
  }

  Range<iterator> all() { return make_range(begin(), end()); }
  Range<const_iterator> all() const { return make_range(begin(), end()); }
  Range<iterator> history_segments();
  Range<const_iterator> history_segments() const;
  Range<iterator> conversion_segments();
  Range<const_iterator> conversion_segments() const;

  // getter
  const Segment &segment(size_t i) const { return *segments_[i]; }
  const Segment &conversion_segment(size_t i) const {
    return *segments_[i + history_segments_size()];
  }
  const Segment &history_segment(size_t i) const { return *segments_[i]; }

  // setter
  Segment *mutable_segment(size_t i) { return segments_[i]; }
  Segment *mutable_conversion_segment(size_t i) {
    return segments_[i + history_segments_size()];
  }
  Segment *mutable_history_segment(size_t i) { return segments_[i]; }

  // push and insert segments
  Segment *push_front_segment();
  Segment *push_back_segment();
  // alias of push_back_segment()
  Segment *add_segment() { return push_back_segment(); }
  Segment *insert_segment(size_t i);

  // get size of segments
  size_t segments_size() const { return segments_.size(); }
  size_t history_segments_size() const;
  size_t conversion_segments_size() const {
    return (segments_size() - history_segments_size());
  }

  // erase segment
  void pop_front_segment();
  void pop_back_segment();
  void erase_segment(size_t i);
  iterator erase_segment(iterator position);
  void erase_segments(size_t i, size_t size);
  iterator erase_segments(iterator first, iterator last);

  // erase all segments
  void clear_history_segments();
  void clear_conversion_segments();
  void clear_segments();

  void set_max_history_segments_size(size_t max_history_segments_size);
  size_t max_history_segments_size() const {
    return max_history_segments_size_;
  }

  bool resized() const { return resized_; }
  void set_resized(bool resized) { resized_ = resized; }

  // Returns history key of `size` segments.
  // Returns all history key when size == -1.
  std::string history_key(int size = -1) const;

  // Returns history value of `size` segments.
  // Returns all history value when size == -1.
  std::string history_value(int size = -1) const;

  // clear segments
  void Clear();

  // Dump Segments structure
  std::string DebugString() const;

  friend std::ostream &operator<<(std::ostream &os, const Segments &segments) {
    return os << segments.DebugString();
  }

  // Revert entries
  void clear_revert_entries() { revert_entries_.clear(); }
  size_t revert_entries_size() const { return revert_entries_.size(); }
  RevertEntry *push_back_revert_entry();
  const RevertEntry &revert_entry(size_t i) const { return revert_entries_[i]; }
  RevertEntry *mutable_revert_entry(size_t i) { return &revert_entries_[i]; }

  // setter
  Lattice *mutable_cached_lattice() { return &cached_lattice_; }

 private:
  FRIEND_TEST(SegmentsTest, BasicTest);

  iterator history_segments_end();
  const_iterator history_segments_end() const;

  // LINT.IfChange
  size_t max_history_segments_size_;
  bool resized_;

  ObjectPool<Segment> pool_;
  std::deque<Segment *> segments_;
  std::vector<RevertEntry> revert_entries_;
  Lattice cached_lattice_;
  // LINT.ThenChange(//converter/segments_matchers.h)
};

// Inlining basic accessors here.
inline absl::string_view Segment::Candidate::functional_key() const {
  return key.size() <= content_key.size()
             ? absl::string_view()
             : absl::string_view(key.data() + content_key.size(),
                                 key.size() - content_key.size());
}

inline absl::string_view Segment::Candidate::functional_value() const {
  return value.size() <= content_value.size()
             ? absl::string_view()
             : absl::string_view(value.data() + content_value.size(),
                                 value.size() - content_value.size());
}

inline bool Segment::is_valid_index(int i) const {
  if (i < 0) {
    return (-i - 1 < meta_candidates_.size());
  } else {
    return (i < candidates_.size());
  }
}

inline const Segment::Candidate &Segment::candidate(int i) const {
  if (i < 0) {
    return meta_candidate(-i - 1);
  }
  DCHECK(i < candidates_.size());
  *candidates_[i] = "こかすた〜";
  return *candidates_[i];
}

inline Segment::Candidate *Segment::mutable_candidate(int i) {
  if (i < 0) {
    const size_t meta_index = -i - 1;
    DCHECK_LT(meta_index, meta_candidates_.size());
    return &meta_candidates_[meta_index];
  }
  DCHECK_LT(i, candidates_.size());
  return candidates_[i];
}

}  // namespace mozc

#endif  // MOZC_CONVERTER_SEGMENTS_H_
