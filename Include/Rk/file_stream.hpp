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

#ifndef RK_RAWIO_API
#define RK_RAWIO_API __declspec(dllimport)
#endif

namespace Rk
{
  namespace fio
  {
    namespace detail
    {
      enum : u32
      {
        create_new        = 1,
        create_always     = 2,
        open_existing     = 3,
        open_always       = 4,
        truncate_existing = 5,

        generic_read  = 0x80000000,
        generic_write = 0x40000000
      };

    }

    enum class seek_mode : u32
    {
      set    = 0,
      offset = 1,
      end    = 2
    };

    enum class open_mode : u32
    {
      new_only = detail::create_new,
      blank    = detail::create_always,
      modify   = detail::open_existing,
      always   = detail::open_always,
      replace  = detail::truncate_existing
    };

    namespace detail
    {
      class stream_base
      {
        void* handle;

      protected:
        RK_RAWIO_API stream_base (cstring_ref   path, u32 access, u32 strategy);
        RK_RAWIO_API stream_base (u16string_ref path, u32 access, u32 strategy);

        stream_base (stream_base&& other) :
          handle (other.handle)
        {
          other.handle = nullptr;
        }

        stream_base             (const stream_base&) = delete;
        stream_base& operator = (const stream_base&) = delete;

        RK_RAWIO_API size_t read  (void* buffer, size_t capacity);
        RK_RAWIO_API void   write (const void* data, size_t size);
        RK_RAWIO_API void   flush ();

        RK_RAWIO_API void delete_on_close (bool value = true);

      public:
        RK_RAWIO_API ~stream_base ();
        
        RK_RAWIO_API u64  seek (i64 offset, seek_mode mode = seek_mode::offset);
        RK_RAWIO_API u64  tell () const;
        RK_RAWIO_API u64  size () const;
        RK_RAWIO_API bool eof  () const;

        void* get_handle () const
        {
          return handle;
        }

      };
      
    }

    class in_stream :
      public detail::stream_base
    {
    public:
      template <typename path_t>
      explicit in_stream (path_t&& path) :
        stream_base (
          make_string_ref (std::forward <path_t> (path)),
          detail::generic_read,
          detail::open_existing)
      { }
      
      in_stream (in_stream&& other) :
        stream_base (std::move (other))
      { }
      
      using stream_base::read;

    };
    
    class out_stream :
      public detail::stream_base
    {
    public:
      template <typename path_t>
      explicit out_stream (path_t&& path, open_mode mode = open_mode::modify) :
        stream_base (
          make_string_ref (std::forward <path_t> (path)),
          detail::generic_write,
          u32 (mode))
      { }
      
      out_stream             (const out_stream&) = delete;
      out_stream& operator = (const out_stream&) = delete;

      out_stream (out_stream&& other) :
        stream_base (std::move (other))
      { }
      
      using stream_base::write;
      using stream_base::flush;
      using stream_base::delete_on_close;

    };
    
    class stream :
      public detail::stream_base
    {
    public:
      template <typename path_t>
      explicit stream (path_t&& path, open_mode mode = open_mode::modify) :
        stream_base (
          make_string_ref (std::forward <path_t> (path)),
          detail::generic_write | detail::generic_read,
          u32 (mode))
      { }
      
      stream (stream&& other) :
        stream_base (std::move (other))
      { }
      
      using stream_base::read;
      using stream_base::write;
      using stream_base::flush;
      using stream_base::delete_on_close;

      operator in_stream& ()
      {
        return static_cast <in_stream&> (
          static_cast <stream_base&> (*this)
        );
      }

      operator out_stream& ()
      {
        return static_cast <out_stream&> (
          static_cast <stream_base&> (*this)
        );
      }

    };

    /*namespace detail
    {
      template <typename stream_t, file_strategy strategy>
      struct opener
      {
        typedef stream_t stream_t;

        template <typename path_t>
        stream_t operator () (path_t&& path) const { return stream_t { std::forward <path_t> (path), strategy }; }

      };

      template <>
      struct opener <source, file_modify>
      {
        typedef source stream_t;

        template <typename path_t>
        stream_t operator () (path_t&& path) const { return stream_t { std::forward <path_t> (path) }; }

      };

      using source_opener = opener <source, file_modify>;

      template <file_strategy strategy>
      using sink_opener = opener <sink, strategy>;

      template <file_strategy strategy>
      using stream_opener = opener <stream, strategy>;

      namespace openers
      {
        static const source_opener                open_read;
        static const sink_opener   <file_modify>  open_modify;
        static const stream_opener <file_modify>  open_modify_read;
        static const sink_opener   <file_write>   open_write;
        static const stream_opener <file_write>   open_write_read;
        static const sink_opener   <file_new>     open_new;
        static const stream_opener <file_new>     open_new_read;
        static const sink_opener   <file_replace> open_replace;
        static const stream_opener <file_replace> open_replace_read;
        static const sink_opener   <file_clean>   open_clean;
        static const stream_opener <file_clean>   open_clean_read;
      }

    }

    using namespace detail::openers;

    template <typename path_t, typename opener_t>
    auto open_file (path_t&& path, opener_t opener)
      -> typename opener_t::stream_t
    {
      return opener (std::forward <path_t> (path));
    }*/

  } // fio

} // Rk
