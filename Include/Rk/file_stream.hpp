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

namespace Rk
{
  namespace fio
  {
    namespace stream_private
    {
      extern "C"
      {
        #define fromdll(type) __declspec(dllimport) type __stdcall
        fromdll (void*) CreateFileA                (const char*, u32, u32, void*, u32, u32, void*);
        fromdll (void*) CreateFileW                (const wchar_t*, u32, u32, void*, u32, u32, void*);
        fromdll (void)  CloseHandle                (void*);
        fromdll (i32)   ReadFile                   (void*, void*, u32, u32*, void*);
        fromdll (i32)   WriteFile                  (void*, const void*, u32, u32*, void*);
        fromdll (i32)   SetFilePointerEx           (void*, i64, i64*, u32);
        fromdll (i32)   GetFileSizeEx              (void*, i64*);
        fromdll (i32)   FlushFileBuffers           (void*);
        fromdll (i32)   SetFileInformationByHandle (void*, u32, void*, u32);
        fromdll (u32)   GetLastError               ();
        #undef fromdll
      }

      static inline void* CreateFile (const char* path, u32 acc, u32 share, u32 strat, u32 attr)
      {
        return CreateFileA (path, acc, share, nullptr, strat, attr, nullptr);
      }

      static inline void* CreateFile (const wchar_t* path, u32 acc, u32 share, u32 strat, u32 attr)
      {
        return CreateFileW (path, acc, share, nullptr, strat, attr, nullptr);
      }

      static inline void* CreateFile (const char16* path, u32 acc, u32 share, u32 strat, u32 attr)
      {
        return CreateFile ((wchar_t*) path, acc, share, strat, attr);
      }

      enum : u32
      {
        error_handle_eof = 38,

        create_new        = 1,
        create_always     = 2,
        open_existing     = 3,
        open_always       = 4,
        truncate_existing = 5,

        generic_read  = 0x80000000,
        generic_write = 0x40000000,

        strategy_mask = 0x00000007,
        access_mask   = 0xc0000000,

        file_share_read = 0x1,

        file_attribute_normal = 0x80,

        file_disposition_info = 4
      };

    }

    enum seek_mode : u32
    {
      seek_set    = 0,
      seek_offset = 1,
      seek_end    = 2
    };

    enum file_strategy
    {
      file_modify  = stream_private::open_existing,
      file_write   = stream_private::open_always,
      file_new     = stream_private::create_new,
      file_replace = stream_private::truncate_existing,
      file_clean   = stream_private::create_always
    };

    class stream_base
    {
      void* handle;

      template <typename char_t>
      static void* open (string_ref_base <char_t> path, u32 access, u32 strategy)
      {
        using namespace stream_private;

        auto handle = CreateFile (
          path.data (),
          access,
          file_share_read,
          strategy,
          file_attribute_normal/* | access_hint*/
        );

        if (!handle || handle == (void*) ~uptr (0))
          throw win_error ("CreateFile failed");

        return handle;
      }

    protected:
      template <typename char_t>
      stream_base (string_ref_base <char_t> path, u32 access, u32 strategy) :
        handle (open (path, access, strategy))
      { }
      
      stream_base (stream_base&& other) :
        handle (other.handle)
      {
        other.handle = nullptr;
      }

      stream_base             (const stream_base&) = delete;
      stream_base& operator = (const stream_base&) = delete;

      size_t read (void* buffer, size_t capacity)
      {
        using namespace stream_private;

        if (!buffer)
          throw std::invalid_argument ("Null buffer");

        if (capacity == 0)
          return 0;

        u32 bytes_read = 0;
        auto ok = ReadFile (get_handle (), buffer, capacity, &bytes_read, 0);
        if (!ok)
        {
          auto err = GetLastError ();
          if (err != error_handle_eof)
            throw win_error ("ReadFile failed", err);
        }

        return bytes_read;
      }

      void write (const void* data, size_t size)
      {
        using namespace stream_private;

        if (size && !data)
          throw std::invalid_argument ("Null data");

        if (size == 0)
          return;

        u32 bytes_written = 0;
        auto ok = WriteFile (get_handle (), data, size, &bytes_written, 0);
        if (!ok)
          throw win_error ("WriteFile failed");
      }
    
      void flush ()
      {
        stream_private::FlushFileBuffers (get_handle ());
      }

      void delete_on_close (bool value = true)
      {
        using namespace stream_private;

        i32 info = value ? 1 : 0;
        auto ok = SetFileInformationByHandle (handle, file_disposition_info, &info, sizeof (info));
        if (!ok)
          throw win_error ("SetFileInformationByHandle failed");
      }

      void retain_on_close (bool value = true)
      {
        delete_on_close (!value);
      }

    public:
      ~stream_base ()
      {
        stream_private::CloseHandle (handle);
      }
    
      u64 seek (i64 offset, seek_mode mode = seek_offset)
      {
        using namespace stream_private;

        if (mode == seek_set && offset < 0)
          throw std::invalid_argument ("Cannot seek before start of file");
        else if (mode == seek_end && offset > 0)
          throw std::invalid_argument ("Cannot seek beyond end of file");

        i64 new_position;
        auto ok = SetFilePointerEx (handle, offset, &new_position, mode);
        if (!ok || new_position < 0)
          throw win_error ("SetFilePointerEx failed");

        return new_position;
      }

      u64 tell () const
      {
        using namespace stream_private;

        i64 position;
        auto ok = SetFilePointerEx (handle, 0, &position, seek_offset);
        if (!ok || position < 0)
          throw win_error ("SetFilePointerEx failed");

        return position;
      }

      u64 size () const
      {
        using namespace stream_private;

        i64 bytes;
        auto ok = GetFileSizeEx (handle, &bytes);
        if (!ok || bytes < 0)
          throw win_error ("GetFileSizeEx failed");

        return bytes;
      }

      bool eof () const
      {
        return tell () == size ();
      }

      void* get_handle ()
      {
        return handle;
      }

    };
  
    class source :
      public stream_base
    {
    public:
      template <typename path_t>
      explicit source (path_t&& path) :
        stream_base (
          make_string_ref (std::forward <path_t> (path)),
          stream_private::generic_read,
          stream_private::open_existing
        )
      { }
      
      source (source&& other) :
        stream_base (std::move (other))
      { }
      
      using stream_base::read;

    };
  
    class sink :
      public stream_base
    {
    public:
      template <typename path_t>
      explicit sink (path_t&& path, file_strategy strat = file_modify) :
        stream_base (
          make_string_ref (std::forward <path_t> (path)),
          stream_private::generic_write,
          strat
        )
      { }
      
      sink (sink&& other) :
        stream_base (std::move (other))
      { }
      
      using stream_base::write;
      using stream_base::flush;
      using stream_base::delete_on_close;
      using stream_base::retain_on_close;

    };
  
    class stream :
      public stream_base
    {
    public:
      template <typename path_t>
      explicit stream (path_t&& path, file_strategy strat = file_modify) :
        stream (
          make_string_ref (std::forward <path_t> (path)),
          stream_private::generic_write |
          stream_private::generic_read,
          strat
        )
      { }
      
      stream (stream&& other) :
        stream (std::move (other))
      { }
      
      using stream_base::read;
      using stream_base::write;
      using stream_base::flush;
      using stream_base::delete_on_close;
      using stream_base::retain_on_close;

      operator source& ()
      {
        return static_cast <source&> (
          static_cast <stream_base&> (*this)
        );
      }

      operator sink& ()
      {
        return static_cast <sink&> (
          static_cast <stream_base&> (*this)
        );
      }

    };

    namespace stream_private
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

    using namespace stream_private::openers;

    template <typename path_t, typename opener_t>
    auto open_file (path_t&& path, opener_t opener)
      -> typename opener_t::stream_t
    {
      return opener (std::forward <path_t> (path));
    }

  } // fio

} // Rk
