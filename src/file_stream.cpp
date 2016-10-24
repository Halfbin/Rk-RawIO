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

#ifndef _WINDOWS
#include <unistd.h>
#include <fcntl.h>
#endif

namespace Rk {
  namespace fio {
    namespace detail {
#ifdef _WINDOWS
      extern "C" {
        #define fromdll(type) __declspec(dllimport) type __stdcall
        fromdll (void*) CreateFileW                (wchar_t const*, u32, u32, void*, u32, u32, void*);
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

      enum : u32 {
        error_handle_eof = 38,

        create_new = 1,
        create_always = 2,
        open_existing = 3,
        open_always = 4,
        truncate_existing = 5,

        generic_read  = 0x80000000,
        generic_write = 0x40000000,

        file_share_read = 0x1,
        file_attribute_normal = 0x80
      };

      StreamBase::StreamBase (cstring_ref path_in, u32 access, OpenMode mode) {
        auto path = string_utf8_to_16 (path_in);

        u32 win_access = ((access & read_bit )? generic_read  : 0)
                       | ((access & write_bit)? generic_write : 0);

        u32 win_strategy = 0;
        switch (mode) {
          case OpenMode::create:   win_strategy = create_new; break;
          case OpenMode::replace:  win_strategy = create_always; break;
          case OpenMode::existing: win_strategy = open_existing; break;
          case OpenMode::always:   win_strategy = open_always; break;
          case OpenMode::truncate: win_strategy = truncate_existing; break;
        }

        auto handle = CreateFileW (path.c_str (), win_access, file_share_read, win_strategy, file_attribute_normal);
        if (!handle || handle == (void*) ~uptr (0))
          throw WinError ("CreateFile failed");
        return handle;
      }
#else
      StreamBase::StreamBase (cstring_ref path_in, u32 access, OpenMode mode) {
        auto path = to_string (path_in);

        int nix_flags = 0;
        switch (mode) {
          case OpenMode::create:   nix_flags = O_CREAT | O_EXCL; break;
          case OpenMode::replace:  nix_flags = O_CREAT | O_TRUNC; break;
          case OpenMode::existing: break;
          case OpenMode::always:   nix_flags = O_CREAT; break;
          case OpenMode::truncate: nix_flags = O_TRUNC; break;
        }

        if      (access == read_bit ) nix_flags |= O_RDONLY;
        else if (access == write_bit) nix_flags |= O_WRONLY;
        else                          nix_flags |= O_RDWR;

        handle = open (path.c_str (), nix_flags, 0644);
        if (handle == -1)
          throw std::system_error (errno, std::system_category (), "open failed");
      }
#endif

      size_t StreamBase::read (void* buffer, size_t capacity) {
        if (!buffer)
          throw std::invalid_argument ("Null buffer");

        if (capacity == 0)
          return 0;

#ifdef _WINDOWS
        u32 bytes_read = 0;
        auto ok = ReadFile (handle, buffer, capacity, &bytes_read, 0);
        if (!ok) {
          auto err = GetLastError ();
          if (err != error_handle_eof)
            throw WinError ("ReadFile failed", err);
        }
#else
        ssize_t bytes_read = 0;
        do {
          bytes_read = read (handle, buffer, capacity);
        } while (bytes_read == -1 && errno == EINTR);

        if (bytes_read == -1)
          throw std::system_error (errno, std::system_category (), "read failed");
#endif

        return (size_t) bytes_read;
      }

      void StreamBase::write (void const* data, size_t size) {
        if (size && !data)
          throw std::invalid_argument ("Null data");

        if (size == 0)
          return;

#ifdef _WINDOWS
        u32 bytes_written = 0;
        auto ok = WriteFile (handle, data, size, &bytes_written, 0);
        if (!ok)
          throw WinError ("WriteFile failed");
#else
        ssize_t bytes_written = 0;
        while (bytes_written < size) {
          ssize_t result = write (handle, (char*) data + bytes_written, size - bytes_written);
          if (result == -1) {
            if (errno == EINTR)
              continue;
            else
              throw std::system_error (errno, std::system_category (), "write failed");
          }
          bytes_written += result;
        }
#endif
      }

      void StreamBase::flush () {
#ifdef _WINDOWS
        FlushFileBuffers (handle);
#else
        fsync (handle);
#endif
      }

      StreamBase::~StreamBase () {
#ifdef _WINDOWS
        if (handle)
          CloseHandle (handle);
#else
        if (handle != -1)
          close (handle);
#endif
      }

      u64 StreamBase::seek (i64 offset, SeekMode mode) {
        if (mode == seek_mode::set && offset < 0)
          throw std::invalid_argument ("Cannot seek before start of file");
        else if (mode == seek_mode::end && offset > 0)
          throw std::invalid_argument ("Cannot seek beyond end of file");

#ifdef _WINDOWS
        i64 new_position;
        auto ok = SetFilePointerEx (handle, offset, &new_position, u32 (mode));
        if (!ok || new_position < 0)
          throw WinError ("SetFilePointerEx failed");
#else
        off_t new_position = lseek (handle, offset, int (mode));
        if (new_position == -1)
          throw std::system_error (errno, std::system_category (), "lseek failed");
#endif

        return (u64) new_position;
      }

      u64 StreamBase::tell () const {
#ifdef _WINDOWS
        i64 position;
        auto ok = SetFilePointerEx (handle, 0, &position, u32 (SeekMode::offset));
        if (!ok || position < 0)
          throw WinError ("SetFilePointerEx failed");
#else
        off_t position = lseek (handle, 0, SEEK_CUR);
        if (position == -1)
          throw std::system_error (errno, std::system_category (), "lseek failed");
#endif

        return (u64) position;
      }

      u64 StreamBase::size () const {
#ifdef _WINDOWS
        i64 bytes;
        auto ok = GetFileSizeEx (handle, &bytes);
        if (!ok || bytes < 0)
          throw WinError ("GetFileSizeEx failed");
#else
        auto current = tell ();
        u64 bytes = seek (0, SeekMode::end);
        seek (current, SeekMode::set);
#endif

        return (u64) bytes;
      }
    }
  }
}

