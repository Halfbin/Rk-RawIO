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

#include <Rk/Exception.hpp>
#include <Rk/StringRef.hpp>
#include <Rk/RawIO.hpp>
#include <Rk/Types.hpp>

namespace Rk
{
  namespace FileStreamPrivate
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

  enum SeekMode : u32
  {
    seek_set    = 0,
    seek_offset = 1,
    seek_end    = 2
  };

  enum OpenMode
  {
    file_modify  = FileStreamPrivate::open_existing,
    file_write   = FileStreamPrivate::open_always,
    file_new     = FileStreamPrivate::create_new,
    file_replace = FileStreamPrivate::truncate_existing,
    file_clean   = FileStreamPrivate::create_always
  };

  class FileStreamBase
  {
    void* handle;

    template <typename Char>
    static void* open (StringRefBase <Char> path, u32 access, u32 strategy)
    {
      using namespace FileStreamPrivate;

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
    template <typename Char>
    FileStreamBase (StringRefBase <Char> path, u32 access, u32 strategy) :
      handle (open (path, access, strategy))
    { }
    
    FileStreamBase (FileStreamBase&& other) :
      handle (other.handle)
    {
      other.handle = nullptr;
    }

    FileStreamBase             (const FileStreamBase&) = delete;
    FileStreamBase& operator = (const FileStreamBase&) = delete;

    size_t read (void* buffer, size_t capacity)
    {
      using namespace FileStreamPrivate;

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
      using namespace FileStreamPrivate;

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
      FileStreamPrivate::FlushFileBuffers (get_handle ());
    }

    void delete_on_close (bool value = true)
    {
      using namespace FileStreamPrivate;

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
    ~FileStreamBase ()
    {
      FileStreamPrivate::CloseHandle (handle);
    }
    
    u64 seek (i64 offset, SeekMode seek_mode = seek_offset)
    {
      using namespace FileStreamPrivate;

      if (seek_mode == seek_set && offset < 0)
        throw std::invalid_argument ("Cannot seek before start of file");
      else if (seek_mode == seek_end && offset > 0)
        throw std::invalid_argument ("Cannot seek beyond end of file");

      i64 new_position;
      auto ok = SetFilePointerEx (handle, offset, &new_position, seek_mode);
      if (!ok || new_position < 0)
        throw win_error ("SetFilePointerEx failed");

      return new_position;
    }

    u64 tell () const
    {
      using namespace FileStreamPrivate;

      i64 position;
      auto ok = SetFilePointerEx (handle, 0, &position, seek_offset);
      if (!ok || position < 0)
        throw win_error ("SetFilePointerEx failed");

      return position;
    }

    u64 size () const
    {
      using namespace FileStreamPrivate;

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
  
  class InFileStream :
    public FileStreamBase
  {
  public:
    template <typename Path>
    explicit InFileStream (Path&& path) :
      FileStreamBase (
        make_stringref (std::forward <Path> (path)),
        FileStreamPrivate::generic_read,
        FileStreamPrivate::open_existing
      )
    { }
    
    InFileStream (InFileStream&& other) :
      FileStreamBase (std::move (other))
    { }
    
    using FileStreamBase::read;

  };
  
  class OutFileStream :
    public FileStreamBase
  {
  public:
    template <typename Path>
    explicit OutFileStream (Path&& path, OpenMode strat = file_modify) :
      FileStreamBase (
        make_stringref (std::forward <Path> (path)),
        FileStreamPrivate::generic_write,
        strat
      )
    { }
    
    OutFileStream (OutFileStream&& other) :
      FileStreamBase (std::move (other))
    { }
    
    using FileStreamBase::write;
    using FileStreamBase::flush;
    using FileStreamBase::delete_on_close;
    using FileStreamBase::retain_on_close;

  };
  
  class FileStream :
    public FileStreamBase
  {
  public:
    template <typename Path>
    explicit FileStream (Path&& path, OpenMode strat = file_modify) :
      FileStreamBase (
        make_stringref (std::forward <Path> (path)),
        FileStreamPrivate::generic_write | FileStreamPrivate::generic_read,
        strat
      )
    { }
    
    FileStream (FileStream&& other) :
      FileStreamBase (std::move (other))
    { }
    
    using FileStreamBase::read;
    using FileStreamBase::write;
    using FileStreamBase::flush;
    using FileStreamBase::delete_on_close;
    using FileStreamBase::retain_on_close;

    operator InFileStream& ()
    {
      return static_cast <InFileStream&> (
        static_cast <FileStreamBase&> (*this)
      );
    }

    operator OutFileStream& ()
    {
      return static_cast <OutFileStream&> (
        static_cast <FileStreamBase&> (*this)
      );
    }

  };

  namespace FileStreamPrivate
  {
    template <bool read, bool write, OpenMode strategy>
    struct OpenHelper;

    template <>
    struct OpenHelper <true, false, file_modify>
    {
      typedef InFileStream FileType;

      template <typename Path>
      FileType operator () (Path&& path) { return FileType (std::forward <Path> (path)); }

    };

    template <OpenMode strategy>
    struct OpenHelper <false, true, strategy>
    {
      typedef OutFileStream FileType;

      template <typename Path>
      FileType operator () (Path&& path) { return FileType (std::forward <Path> (path), strategy); }

    };

    template <OpenMode strategy>
    struct OpenHelper <true, true, strategy>
    {
      typedef FileStream FileType;

      template <typename Path>
      FileType operator () (Path&& path) { return FileType (std::forward <Path> (path), strategy); }

    };

    namespace OpenHelpers
    {
      static OpenHelper <true,  false, file_modify > open_read;
      static OpenHelper <false, true,  file_modify > open_modify;
      static OpenHelper <true,  true,  file_modify > open_modify_read;
      static OpenHelper <false, true,  file_write  > open_write;
      static OpenHelper <true,  true,  file_write  > open_write_read;
      static OpenHelper <false, true,  file_new    > open_new;
      static OpenHelper <true,  true,  file_new    > open_new_read;
      static OpenHelper <false, true,  file_replace> open_replace;
      static OpenHelper <true,  true,  file_replace> open_replace_read;
      static OpenHelper <false, true,  file_clean  > open_clean;
      static OpenHelper <true,  true,  file_clean  > open_clean_read;
    }

  }

  using namespace FileStreamPrivate::OpenHelpers;

  template <typename Path, bool read, bool write, OpenMode strategy>
  auto open_file (Path&& path, FileStreamPrivate::OpenHelper <read, write, strategy> mode)
    -> typename FileStreamPrivate::OpenHelper <read, write, strategy>::FileType
  {
    return mode (std::forward <Path> (path));
  }

}
