//
// Copyright (C) 2013 Rk
// Permission to use, copy, modify and distribute this file for any purpose is hereby granted without fee or other
// limitation, provided that
//  - this notice is reproduced verbatim in all copies and modifications of this file, and
//  - neither the names nor trademarks of the copyright holder are used to endorse any work which includes or is derived
//    from this file.
// No warranty applies to the use, either of this software, or any modification of it, and Rk disclaims all liabilities
// in relation to such use.
//

#pragma once

#include <Rk/memory.hpp>

#include <type_traits>

namespace Rk {
  template<typename Value>
  static constexpr bool is_dense_trivially_copyable = std::is_trivially_copyable_v<Value> && (sizeof (Value) % std::alignment_of_v<Value> == 0);

  // basic operations
  template<typename Sink>
  void write (Sink& sink, void const* data, size_t length) {
    sink.write (data, length);
  }

  template<typename Sink>
  size_t read (Sink& source, void* buffer, size_t length) {
    return source.read (buffer, length);
  }

  template<typename Stream>
  bool eof (Stream const& stream) {
    return stream.eof ();
  }
  
  // put

  // single
  template<typename Sink, typename Value, typename = std::enable_if_t<std::is_trivially_copyable_v<Value>>>
  void put (Sink& sink, const Value& value) {
    write (sink, &value, sizeof (Value));
  }
  
  // array, count
  template<typename Sink, typename ForwardIterator>
  void put (Sink& sink, ForwardIterator begin, size_t count) {
    while (count--)
      put (sink, *begin++);
  }

  // array, limit
  template<typename Sink, typename ForwardIterator>
  void put (Sink& sink, ForwardIterator begin, ForwardIterator end) {
    while (begin != end)
      put (sink, *begin++);
  }

  // dense array, count
  template<typename Sink, typename Value, typename = std::enable_if_t<is_dense_trivially_copyable<Value>>>
  void put (Sink& sink, Value const* begin, size_t count) {
    write (sink, begin, sizeof (Value) * count);
  }

  // dense array, limit
  template<typename Sink, typename Value, typename = std::enable_if_t<is_dense_trivially_copyable<Value>>>
  void put (Sink& sink, Value const* begin, Value const* end) {
    put (sink, begin, end - begin);
  }

  // get

  // single, by ref
  template<typename Source, typename Value, typename = std::enable_if_t<std::is_trivially_copyable_v<Value>>>
  void get (Source& source, Value& dest) {
    auto bytes = read (source, &dest, sizeof (Value));
    if (bytes != sizeof (Value))
      throw std::runtime_error ("Not enough data to get value");
  }

  // single, by value
  template<typename Value, typename Source, typename = std::enable_if_t<std::is_trivially_copyable_v<Value>>>
  Value get (Source& source) {
    raw_storage<Value> store;
    get (source, store.value ());
    return store.value ();
  }

  // array, count
  template<typename Source, typename OutputIterator>
  void get (Source& source, OutputIterator begin, size_t count) {
    while (count--)
      get (source, *begin++);
  }

  // array, limit
  template<typename Source, typename OutputIterator>
  void get (Source& source, OutputIterator begin, OutputIterator end) {
    while (begin != end)
      get (source, *begin++);
  }

  // dense array, count
  template<typename Source, typename Value, typename = std::enable_if_t<is_dense_trivially_copyable<Value>>>
  void get (Source& source, Value* begin, size_t count) {
    read (source, begin, sizeof (Value) * count);
  }

  // dense array, limit
  template<typename Source, typename Value, typename = std::enable_if_t<is_dense_trivially_copyable<Value>>>
  void get (Source& source, Value* begin, Value* end) {
    get (source, begin, end - begin);
  }
}

