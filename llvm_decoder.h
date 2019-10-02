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

#include <map>
#include <vector>
#include "llvm_bitreader.h"

struct BlockOrRecord
{
  uint32_t id;
  uint32_t blockDwordLength = 0;    // 0 for records

  bool IsBlock() const { return blockDwordLength > 0; }
  bool IsRecord() const { return blockDwordLength == 0; }
  // if a block, the child blocks/records
  std::vector<BlockOrRecord> children;

  // if a record, the ops
  std::vector<uint64_t> ops;
  // if this is an abbreviated record with a blob, this is the last operand
  // this points into the overall byte storage, so the lifetime is limited.
  const byte *blob = NULL;
  size_t blobLength = 0;
};

enum class AbbrevEncoding : uint8_t
{
  Fixed = 1,
  VBR = 2,
  Array = 3,
  Char6 = 4,
  Blob = 5,
  // the abbrev encoding is only 3 bits, so 8 is not representable, we can store whether or not
  // we're a literal this way.
  Literal = 8,
};

struct AbbrevParam
{
  AbbrevEncoding encoding;
  uint64_t value;    // this is also the bitwidth for Fixed/VBR
};

struct AbbrevDesc
{
  std::vector<AbbrevParam> params;
};

// the temporary context while pushing/popping blocks
struct BlockContext
{
  BlockContext(size_t size) : abbrevSize(size) {}
  size_t abbrevSize;
  std::vector<AbbrevDesc> abbrevs;
};

// the permanent block info defined by BLOCKINFO
struct BlockInfo
{
  // std::string blockname;
  // std::vector<std::string> recordnames;
  std::vector<AbbrevDesc> abbrevs;
};

class BitcodeReader
{
public:
  BitcodeReader(const byte *bitcode, size_t length);
  BlockOrRecord ReadToplevelBlock();
  bool AtEndOfStream();

private:
  BitReader b;

  void ReadBlockContents(BlockOrRecord &block);
  const AbbrevDesc &getAbbrev(uint32_t blockId, uint32_t abbrevID);
  size_t abbrevSize() const;
  uint64_t decodeAbbrevParam(const AbbrevParam &param);

  std::vector<BlockContext> blockStack;
  std::map<uint32_t, BlockInfo> blockInfo;
};
