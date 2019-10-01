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

#include "dxil_inspect.h"
#include "common.h"

namespace DXIL
{
struct ProgramHeader
{
  uint32_t ProgramVersion;    // Major and minor version of shader, including type.
  uint32_t SizeInUint32;      // Size in uint32_t units including this header.
  uint32_t DxilMagic;         // 0x4C495844, ASCII "DXIL".
  uint32_t DxilVersion;       // DXIL version.
  uint32_t BitcodeOffset;     // Offset to LLVM bitcode (from DxilMagic).
  uint32_t BitcodeSize;       // Size of LLVM bitcode.
};

Program::Program(const void *bytes, size_t length)
{
  const byte *ptr = (const byte *)bytes;
  const ProgramHeader *header = (const ProgramHeader *)ptr;
  assert(header->DxilMagic == MAKE_FOURCC('D', 'X', 'I', 'L'));

  const byte *bitcode = ((const byte *)&header->DxilMagic) + header->BitcodeOffset;
  assert(bitcode + header->BitcodeSize == ptr + length);
}

struct ILDNHeader
{
  uint16_t Flags;
  uint16_t NameLength;
  char Name[1];
};

DebugName::DebugName(const void *bytes, size_t length)
{
  const ILDNHeader *header = (const ILDNHeader *)bytes;

  flags = header->Flags;
  name = header->Name;
}
};