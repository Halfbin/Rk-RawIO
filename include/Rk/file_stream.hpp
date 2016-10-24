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

#include <Rk/exception.hpp>
#include <Rk/string_ref.hpp>
#include <Rk/raw_io.hpp>
#include <Rk/types.hpp>

namespace Rk {
  namespace fio {
    namespace detail {
      enum : u32 {
        read_bit  = 1,
        write_bit = 2
      };
    }

    enum class SeekMode : u32 {
      set    = 0,
      offset = 1,
      end    = 2
    };

    enum class OpenMode : u32 {
      create,
      truncate,
      replace,
      existing,
      always
    };

    namespace detail {
#ifdef _WINDOWS
      using Handle = void*;
      static constexpr Handle bad_handle = nullptr;
#else
      using Handle = int;
      static constexpr Handle bad_handle = -1;
#endif

      class StreamBase {
        Handle handle;

      protected:
        StreamBase (cstring_ref   path, u32 access, u32 strategy);
        StreamBase (u16string_ref path, u32 access, u32 strategy);

        StreamBase (StreamBase&& other) :
          handle (other.handle)
        {
          other.handle = bad_handle;
        }

        StreamBase             (StreamBase const&) = delete;
        StreamBase& operator = (StreamBase const&) = delete;

        size_t read  (void* buffer, size_t capacity);
        void   write (void const* data, size_t size);
        void   flush ();

      public:
        ~StreamBase ();

        u64  seek (i64 offset, SeekMode mode = SeekMode::offset);
        u64  tell () const;
        u64  size () const;

        Handle get_handle () const {
          return handle;
        }
      };
    }

    class InStream : public detail::StreamBase {
    public:
      template<typename Path>
      explicit InStream (Path&& path) :
        StreamBase (make_string_ref (std::forward<Path> (path)), detail::read_bit, OpenMode::existing)
      { }

      InStream (InStream&& other) = default;

      using StreamBase::read;
    };

    class OutStream : public detail::StreamBase {
    public:
      template<typename Path>
      explicit OutStream (Path&& path, OpenMode mode = OpenMode::existing) :
        StreamBase (make_string_ref (std::forward<Path> (path)), detail::write_bit, mode)
      { }

      OutStream (OutStream&& other) = default;

      using StreamBase::write;
      using StreamBase::flush;
    };

    class Stream : public detail::StreamBase {
    public:
      template<typename Path>
      explicit Stream (Path&& path, OpenMode mode = OpenMode::existing) :
        StreamBase (make_string_ref (std::forward<Path> (path)), detail::write_bit | detail::read_bit, mode)
      { }

      Stream (Stream&& other) = default;

      using StreamBase::read;
      using StreamBase::write;
      using StreamBase::flush;

      operator InStream& () {
        return static_cast<InStream&> (
          static_cast<StreamBase&> (*this)
        );
      }

      operator OutStream& () {
        return static_cast<OutStream&> (
          static_cast<StreamBase&> (*this)
        );
      }
    };
  }
}

