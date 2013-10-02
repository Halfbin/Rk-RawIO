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

#include <type_traits>

namespace Rk
{
  // Basic operations
  template <typename Sink>
  void write (Sink& sink, const void* data, size_t length)
  {
    sink.write (data, length);
  }

  template <typename Source>
  size_t read (Source& source, void* buffer, size_t length)
  {
    return source.read (buffer, length);
  }

  template <typename Stream>
  bool eof (const Stream& stream)
  {
    return stream.eof ();
  }
  
  // Put
  template <typename Sink, typename T>
  auto put (Sink& sink, const T& value)
    -> std::enable_if_t <
      std::is_trivially_copyable <T>::value
    >
  {
    write (sink, &value, sizeof (T));
  }

  template <typename Sink, typename Iter>
  void put (Sink& sink, Iter begin, Iter end)
  {
    while (begin != end)
      put (sink, *begin++);
  }

  template <typename Sink, typename T>
  auto put (Sink& sink, const T* begin, const T* end)
    -> std::enable_if_t <
      std::is_trivially_copyable     <T>::value &&
      sizeof (T) % std::alignment_of <T>::value == 0
    >
  {
    write (sink, begin, sizeof (T) * (end - begin));
  }

  template <typename Sink, typename Iter>
  void put (Sink& sink, Iter begin, size_t count)
  {
    put (sink, begin, begin + count);
  }

  // Get
  template <typename Source, typename T>
  auto get (Source& source, T& dest)
    -> std::enable_if_t <
      std::is_trivially_copyable <T>::value
    >
  {
    auto bytes = read (source, &dest, sizeof (T));
    if (bytes != sizeof (T))
      throw std::runtime_error ("Not enough data to get value");
  }

  template <typename Source, typename Iter>
  void get (Source& source, Iter begin, Iter end)
  {
    while (begin != end)
      get (source, *begin++);
  }

  template <typename T, typename Source>
  auto get (Source& source)
    -> std::enable_if_t <
      std::is_trivially_copyable <T>::value,
      T
    >
  {
    RawStorage <T> store;
    get (source, store.value ());
    return store.value ();
  }

  template <typename Source, typename T>
  auto get (Source& source, T* begin, T* end)
    -> std::enable_if_t <
      std::is_trivially_copyable     <T>::value &&
      sizeof (T) % std::alignment_of <T>::value == 0
    >
  {
    read (source, begin, sizeof (T) * (end - begin));
  }

  template <typename Source, typename Iter>
  void get (Source& source, Iter begin, size_t count)
  {
    get (source, begin, begin + count);
  }

}
