/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <string.h>
#include "common.h"

class BitReader
{
public:
  BitReader(const byte *bits, size_t length)
      : m_Bits(bits), m_Start(bits), m_End(bits + length), m_Offset(0)
  {
  }
  size_t ByteOffset() { return m_Bits - m_Start; }
  size_t BitOffset() { return ByteOffset() * 8 + m_Offset; }
  size_t ByteLength() { return m_End - m_Start; }
  char c6()
  {
    byte c = 0;
    ReadBits(6, &c);

    if(c >= 0 && c <= 25)
      return char('a' + c);
    else if(c >= 26 && c <= 51)
      return char('A' + c - 26);
    else if(c >= 52 && c <= 61)
      return char('0' + c - 52);
    else if(c == 62)
      return '.';
    else if(c == 63)
      return '_';

    assert(false && "Unexpected 6-bit char");

    return '?';
  }

  template <typename T>
  T fixed(const size_t bitWidth)
  {
    byte scratch[8] = {};

    assert(bitWidth <= 64);

    ReadBits(bitWidth, scratch);

    T ret;
    memcpy(&ret, scratch, sizeof(T));
    return ret;
  }

  template <typename T>
  T vbr(const size_t groupBitSize)
  {
    uint64_t ret = 0;

    assert(groupBitSize <= 8 && "Only chunk sizes up to 8 supported");
    byte scratch = 0;

    const byte hibit = 1 << (groupBitSize - 1);
    const byte lobits = hibit - 1;

    uint64_t shift = 0;
    do
    {
      ReadBits(groupBitSize, &scratch);

      assert(groupBitSize + shift <= 64);

      ret += (uint64_t(scratch & lobits) << shift);

      shift += uint64_t(groupBitSize - 1);
    } while(scratch & hibit);

    // check for overflow of the return type
    const uint64_t mask = ((1ULL << (sizeof(T) * 8 - 1)) - 1) << 1 | 1;
    assert((ret & mask) == ret);

    return T(ret);
  }

  template <typename T>
  T svbr(size_t groupBitSize)
  {
    // the value will fit in a uint64_t because the any negative values with the high bit set, which
    // would overflow when shifted, no longer have the high bit set after being negated.
    uint64_t var = vbr<uint64_t>(groupBitSize);

    // if the low bit is set, it's negative
    if(var & 0x1)
    {
      return T(-int64_t(var >> 1));
    }
    else
    {
      return T(var >> 1);
    }
  }

  template <typename T>
  T Read()
  {
    byte scratch[sizeof(T)] = {};

    ReadBits(sizeof(T) * 8, scratch);

    T ret;
    memcpy(&ret, scratch, sizeof(T));
    return ret;
  }

  void ReadBlob(const byte *&blobptr, size_t &bloblen)
  {
    // get the blob length
    bloblen = vbr<size_t>(6);

    // align to dword boundary
    align32bits();

    // the blob is at m_Bits now
    blobptr = m_Bits;

    // advance by the length, and align up as well
    m_Bits += bloblen;
    align32bits();
  }

  void align32bits()
  {
    // skip the rest of the current byte, if we're part-way through
    if(m_Offset > 0)
      Advance(8 - m_Offset);

    const size_t byteOffs = ByteOffset();
    const size_t alignedByteOffs = (byteOffs + 0x3) & ~0x3;

    // advance by N bytes to dword align the stream
    m_Bits += (alignedByteOffs - byteOffs);
  }

private:
  const byte *m_Bits, *m_Start, *m_End;
  size_t m_Offset;

  void Advance(size_t N)
  {
    m_Offset += N;
    // shouldn't read more than this byte
    assert(m_Offset <= 8);

    // roll over to next byte after consuming all 8 bits
    if(m_Offset == 8)
    {
      m_Bits++;
      m_Offset = 0;
    }
  }

  void ReadBits(size_t remaining, byte *dst)
  {
    size_t dstoffs = 0;

    // if we're already partway through a byte, read as many bits as we need and we can
    if(m_Offset != 0)
    {
      const size_t avail = 8 - m_Offset;

      if(avail == remaining)
      {
        // if we have just enough in this byte, great! shift and mask off, and update the offset

        // grab the bits into the low end
        *dst = (*m_Bits >> m_Offset);

        Advance(remaining);

        return;
      }
      else if(avail > remaining)
      {
        // we have more than enough. Similar to above but we need to mask out only the bits we need.

        // grab the bits into the low end and mask
        *dst = (*m_Bits >> m_Offset) & ((1 << remaining) - 1);

        Advance(remaining);

        return;
      }
      else
      {
        // we don't have enough. Consume what we can then continue
        *dst = (*m_Bits >> m_Offset);

        dstoffs = avail;
        remaining -= avail;

        Advance(avail);
      }
    }
    else
    {
      // ensure if we didn't read any bits that the byte is zeroed out, so we can OR on bits below
      // without needing to worry
      *dst = 0;
    }

    // we're now at the start of a byte since we read any remainder above.
    assert(m_Offset == 0);

    // if we have to read whole bytes, do that here.
    if(remaining >= 8)
    {
      if(dstoffs == 0)
      {
        // if dstoffs is 0 then it's an easy case, we can just copy all the whole bytes into *dst
        memcpy(dst, m_Bits, remaining / 8);

        // manual advance
        m_Bits += (remaining / 8);

        // make sure we read any sub-byte remainder
        remaining &= 0x7;
      }
      else
      {
        while(remaining > 8)
        {
          // manual advance
          const byte cur = *m_Bits;
          m_Bits++;

          remaining -= 8;

          // dstoffs doesn't change because we wrap around to the same offset in the next byte.
          // However we do need to shuffle the bits in cur around to add what will fit into the
          // current byte, and then the remainder into the next byte.
          *dst |= (cur << dstoffs);
          dst++;
          *dst = (cur >> (8 - dstoffs));
        }
      }
    }

    // if nothing remains, return
    if(remaining == 0)
      return;

    // we should now have no more than than 7 bits to read
    assert(remaining < 8);

    // this is the mask to get only the bits we want
    const byte mask = ((1 << remaining) - 1);

    // take the bits that we want from the next byte (knowing we want the low-order bits), and shift
    // them into where they should go.
    *dst |= (*m_Bits & mask) << dstoffs;

    // consume the bits we used
    Advance(remaining);
  }
};
