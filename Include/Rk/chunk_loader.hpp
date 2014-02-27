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

#include <Rk/types.hpp>

#ifndef RK_RAWIO_API
#define RK_RAWIO_API __declspec(dllimport)
#endif

namespace Rk
{
  namespace fio
  {
    #define CHUNK_TYPE_U32(a, b, c, d) (u32 (d) << 24 | u32 (c) << 16 | u32 (b) << 8 | u32 (a))

    template <typename src_t>
    class chunk_loader
    {
      u32 ty, sz;

    public:
      src_t& source;

      chunk_loader (src_t& new_source) :
        source (new_source)
      { }

      u32 chunk_type () const
      {
        return ty;
      }

      u32 chunk_size () const
      {
        return sz;
      }

      bool resume ()
      {
        if (source.eof ())
          return false;

        get (source, ty);
        get (source, sz);
        return true;
      }

    };

    template <typename src_t>
    auto make_chunk_loader (src_t& source)
    {
      return chunk_loader <src_t> (source);
    }

  }

}
