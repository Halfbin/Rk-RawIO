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

// Implements
#include <Rk/file_stream.hpp>

// Uses
#include <Rk/unicode_convert.hpp>

namespace Rk
{
  namespace fio
  {
    namespace detail
    {
      extern "C"
      {
        #define fromdll(type) __declspec(dllimport) type __stdcall
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

      enum : u32
      {
        error_handle_eof = 38,

        strategy_mask = 0x00000007,
        access_mask   = 0xc0000000,

        file_share_read = 0x1,

        file_attribute_normal = 0x80,

        file_disposition_info = 4
      };

      static void* create_file (u16string_ref path, u32 acc, u32 share, u32 strat, u32 attr)
      {
        auto str_path = to_string (path);
        return CreateFileW ((const wchar_t*) str_path.c_str (), acc, share, nullptr, strat, attr, nullptr);
      }

      static void* create_file (cstring_ref path, u32 acc, u32 share, u32 strat, u32 attr)
      {
        auto u16_path = string_utf8_to_16 (path);
        return CreateFileW ((const wchar_t*) u16_path.c_str (), acc, share, nullptr, strat, attr, nullptr);
      }

      stream_base::stream_base (cstring_ref path, u32 access, u32 strategy)
      {
        handle = create_file (
          path,
          access,
          file_share_read,
          strategy,
          file_attribute_normal/* | access_hint*/
        );

        if (!handle || handle == (void*) ~uptr (0))
          throw win_error ("CreateFile failed");
      }

      stream_base::stream_base (u16string_ref path, u32 access, u32 strategy)
      {
        handle = create_file (
          path,
          access,
          file_share_read,
          strategy,
          file_attribute_normal/* | access_hint*/
        );

        if (!handle || handle == (void*) ~uptr (0))
          throw win_error ("CreateFile failed");
      }

      size_t stream_base::read (void* buffer, size_t capacity)
      {
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

      void stream_base::write (const void* data, size_t size)
      {
        if (size && !data)
          throw std::invalid_argument ("Null data");

        if (size == 0)
          return;

        u32 bytes_written = 0;
        auto ok = WriteFile (get_handle (), data, size, &bytes_written, 0);
        if (!ok)
          throw win_error ("WriteFile failed");
      }

      void stream_base::flush ()
      {
        FlushFileBuffers (get_handle ());
      }

      void stream_base::delete_on_close (bool value)
      {
        i32 info = value ? 1 : 0;
        auto ok = SetFileInformationByHandle (handle, file_disposition_info, &info, sizeof (info));
        if (!ok)
          throw win_error ("SetFileInformationByHandle failed");
      }

      stream_base::~stream_base ()
      {
        if (handle)
          CloseHandle (handle);
      }
      
      u64 stream_base::seek (i64 offset, seek_mode mode)
      {
        if (mode == seek_mode::set && offset < 0)
          throw std::invalid_argument ("Cannot seek before start of file");
        else if (mode == seek_mode::end && offset > 0)
          throw std::invalid_argument ("Cannot seek beyond end of file");

        i64 new_position;
        auto ok = SetFilePointerEx (handle, offset, &new_position, u32 (mode));
        if (!ok || new_position < 0)
          throw win_error ("SetFilePointerEx failed");

        return new_position;
      }

      u64 stream_base::tell () const
      {
        i64 position;
        auto ok = SetFilePointerEx (handle, 0, &position, u32 (seek_mode::offset));
        if (!ok || position < 0)
          throw win_error ("SetFilePointerEx failed");

        return position;
      }

      u64 stream_base::size () const
      {
        i64 bytes;
        auto ok = GetFileSizeEx (handle, &bytes);
        if (!ok || bytes < 0)
          throw win_error ("GetFileSizeEx failed");

        return bytes;
      }

      bool stream_base::eof () const
      {
        return tell () == size ();
      }

    }

  }

}
