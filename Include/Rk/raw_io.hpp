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

namespace Rk
{
  template <typename value_t>
  struct is_dense_trivially_copyable :
    std::integral_constant <
      bool,
      std::is_trivially_copyable           <value_t>::value &&
      sizeof (value_t) % std::alignment_of <value_t>::value == 0
    >
  { };

  // basic operations
  template <typename sink_t>
  void write (sink_t& sink, const void* data, size_t length)
  {
    sink.write (data, length);
  }

  template <typename source_t>
  size_t read (source_t& source, void* buffer, size_t length)
  {
    return source.read (buffer, length);
  }

  template <typename stream_t>
  bool eof (const stream_t& stream)
  {
    return stream.eof ();
  }
  
  // put

  // single
  template <typename sink_t, typename value_t>
  auto put (sink_t& sink, const value_t& value)
    -> std::enable_if_t <std::is_trivially_copyable <value_t>::value>
  {
    write (sink, &value, sizeof (value_t));
  }
  
  // array, count
  template <typename sink_t, typename forward_iter_t>
  void put (sink_t& sink, forward_iter_t begin, size_t count)
  {
    while (count--)
      put (sink, *begin++);
  }

  // array, limit
  template <typename sink_t, typename forward_iter_t>
  void put (sink_t& sink, forward_iter_t begin, forward_iter_t end)
  {
    while (begin != end)
      put (sink, *begin++);
  }

  // dense array, count
  template <typename sink_t, typename value_t>
  auto put (sink_t& sink, const value_t* begin, size_t count)
    -> std::enable_if_t <is_dense_trivially_copyable <value_t>::value>
  {
    write (sink, begin, sizeof (value_t) * count);
  }

  // dense array, limit
  template <typename sink_t, typename value_t>
  auto put (sink_t& sink, const value_t* begin, const value_t* end)
    -> std::enable_if_t <is_dense_trivially_copyable <value_t>::value>
  {
    put (sink, begin, end - begin);
  }

  // get

  // single, by ref
  template <typename source_t, typename value_t>
  auto get (source_t& source, value_t& dest)
    -> std::enable_if_t <std::is_trivially_copyable <value_t>::value>
  {
    auto bytes = read (source, &dest, sizeof (value_t));
    if (bytes != sizeof (value_t))
      throw std::runtime_error ("Not enough data to get value");
  }

  // single, by value
  template <typename value_t, typename source_t>
  auto get (source_t& source)
    -> std::enable_if_t <
      std::is_trivially_copyable <value_t>::value,
      value_t
    >
  {
    raw_storage <value_t> store;
    get (source, store.value ());
    return store.value ();
  }

  // array, count
  template <typename source_t, typename out_iter_t>
  void get (source_t& source, out_iter_t begin, size_t count)
  {
    while (count--)
      get (source, *begin++);
  }

  // array, limit
  template <typename source_t, typename out_iter_t>
  void get (source_t& source, out_iter_t begin, out_iter_t end)
  {
    while (begin != end)
      get (source, *begin++);
  }

  // dense array, count
  template <typename source_t, typename value_t>
  auto get (source_t& source, value_t* begin, size_t count)
    -> std::enable_if_t <is_dense_trivially_copyable <value_t>::value>
  {
    read (source, begin, sizeof (value_t) * count);
  }

  // dense array, limit
  template <typename source_t, typename value_t>
  auto get (source_t& source, value_t* begin, value_t* end)
    -> std::enable_if_t <is_dense_trivially_copyable <value_t>::value>
  {
    get (source, begin, end - begin);
  }

}
