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

#include "llvm_decoder.h"

enum AbbrevId
{
  END_BLOCK = 0,
  ENTER_SUBBLOCK = 1,
  DEFINE_ABBREV = 2,
  UNABBREV_RECORD = 3,
  APPLICATION_ABBREV = 4,
};

enum class BlockInfoRecord
{
  SETBID = 1,
  BLOCKNAME = 2,
  SETRECORDNAME = 3,
};

BitcodeReader::BitcodeReader(const byte *bitcode, size_t length) : b(bitcode, length)
{
  uint32_t magic = b.Read<uint32_t>();

  assert(magic == MAKE_FOURCC('B', 'C', 0xC0, 0xDE));
}

BlockOrRecord BitcodeReader::ReadToplevelBlock()
{
  BlockOrRecord ret;

  // should hit ENTER_SUBBLOCK first for top-level block
  uint32_t abbrevID = b.fixed<uint32_t>(abbrevSize());
  assert(abbrevID == ENTER_SUBBLOCK);

  ReadBlockContents(ret);

  return ret;
}

bool BitcodeReader::AtEndOfStream()
{
  return b.ByteOffset() == b.ByteLength();
}

void BitcodeReader::ReadBlockContents(BlockOrRecord &block)
{
  block.id = b.vbr<uint32_t>(8);

  blockStack.push_back(BlockContext(b.vbr<size_t>(4)));

  b.align32bits();
  block.blockDwordLength = b.Read<uint32_t>();

  // used for blockinfo only
  BlockInfo *curBlockInfo = NULL;

  uint32_t abbrevID = ~0U;
  do
  {
    abbrevID = b.fixed<uint32_t>(abbrevSize());

    if(abbrevID == END_BLOCK)
    {
      b.align32bits();
    }
    else if(abbrevID == ENTER_SUBBLOCK)
    {
      BlockOrRecord sub;

      ReadBlockContents(sub);

      block.children.push_back(sub);
    }
    else if(abbrevID == DEFINE_ABBREV)
    {
      AbbrevDesc a;

      uint32_t numops = b.vbr<uint32_t>(5);

      a.params.resize(numops);

      for(uint32_t i = 0; i < numops; i++)
      {
        AbbrevParam &param = a.params[i];

        bool lit = b.fixed<bool>(1);

        if(lit)
        {
          param.encoding = AbbrevEncoding::Literal;
          param.value = b.vbr<uint64_t>(8);
        }
        else
        {
          param.encoding = b.fixed<AbbrevEncoding>(3);

          if(param.encoding == AbbrevEncoding::Fixed || param.encoding == AbbrevEncoding::VBR)
          {
            param.value = b.vbr<uint64_t>(5);
          }
        }
      }

      if(curBlockInfo)
        curBlockInfo->abbrevs.push_back(a);
      else
        blockStack.back().abbrevs.push_back(a);
    }
    else if(abbrevID == UNABBREV_RECORD)
    {
      BlockOrRecord r;
      r.id = b.vbr<uint32_t>(6);
      uint32_t numops = b.vbr<uint32_t>(6);
      r.ops.resize(numops);
      for(uint32_t i = 0; i < numops; i++)
        r.ops[i] = b.vbr<uint64_t>(6);

      if(block.id == 0)    // BLOCKINFO is block 0
      {
        switch(BlockInfoRecord(r.id))
        {
          case BlockInfoRecord::SETBID:
          {
            curBlockInfo = &blockInfo[(uint32_t)r.ops[0]];
            break;
          }
          case BlockInfoRecord::BLOCKNAME:
          {
            // skipped because this is so rarely used
            /*
            for(uint32_t i = 0; i < r.ops.size(); i++)
              curBlockInfo->blockname.push_back((char)r.ops[i]);
              */
            break;
          }
          case BlockInfoRecord::SETRECORDNAME:
          {
            // skipped because this is so rarely used
            /*
            uint32_t record = (uint32_t)r.ops[0];
            if(record >= curBlockInfo->recordnames.size())
              curBlockInfo->recordnames.resize(record + 1);
            r.ops.erase(r.ops.begin());
            for(uint32_t i = 0; i < r.ops.size(); i++)
              curBlockInfo->recordnames[record].push_back((char)r.ops[i]);
              */
            break;
          }
        }
      }

      block.children.push_back(r);
    }
    else
    {
      const AbbrevDesc &a = getAbbrev(block.id, abbrevID);

      BlockOrRecord r;

      // should have at least one param for the code itself
      assert(!a.params.empty());

      r.id = (uint32_t)decodeAbbrevParam(a.params[0]);

      // process the rest of the operands - since some might be arrays we don't know until we
      // process it how many ops the record will end up with but it will be at least one per
      // parameter.
      r.ops.reserve(a.params.size() - 1);
      for(size_t i = 1; i < a.params.size(); i++)
      {
        const AbbrevParam &param = a.params[i];

        if(param.encoding == AbbrevEncoding::Array)
        {
          // must be another param to specify the value type, and it must be the last
          assert(i + 1 == a.params.size() - 1);
          const AbbrevParam &elType = a.params[i + 1];

          size_t arrayLen = b.vbr<size_t>(6);

          for(size_t el = 0; el < arrayLen; el++)
            r.ops.push_back(decodeAbbrevParam(elType));

          break;
        }
        else if(param.encoding == AbbrevEncoding::Blob)
        {
          // blob must be the last value
          assert(i == a.params.size() - 1);
          b.ReadBlob(r.blob, r.blobLength);

          break;
        }
        else
        {
          r.ops.push_back(decodeAbbrevParam(param));
        }
      }

      block.children.push_back(r);
    }
  } while(abbrevID != END_BLOCK);

  blockStack.pop_back();
}

uint64_t BitcodeReader::decodeAbbrevParam(const AbbrevParam &param)
{
  assert(param.encoding != AbbrevEncoding::Array && param.encoding != AbbrevEncoding::Blob);

  switch(param.encoding)
  {
    case AbbrevEncoding::Fixed: return b.fixed<uint64_t>(param.value);
    case AbbrevEncoding::VBR: return b.vbr<uint64_t>(param.value);
    case AbbrevEncoding::Char6: return b.c6();
    case AbbrevEncoding::Literal: return param.value;
    case AbbrevEncoding::Array:
    case AbbrevEncoding::Blob: assert(false && "These must be decoded specially");
  }

  return 0;
}

size_t BitcodeReader::abbrevSize() const
{
  if(blockStack.empty())
    return 2;
  return blockStack.back().abbrevSize;
}

const AbbrevDesc &BitcodeReader::getAbbrev(uint32_t blockId, uint32_t abbrevID)
{
  const BlockInfo &info = blockInfo[blockId];

  // IDs start at the first application specified ID. Rebase to that to get 0-base indices
  assert(abbrevID >= APPLICATION_ABBREV);
  abbrevID -= APPLICATION_ABBREV;

  // IDs are first assigned to those permanently from BLOCKINFO
  if(abbrevID < info.abbrevs.size())
    return info.abbrevs[abbrevID];

  // block-local IDs start after the BLOCKINFO ones
  abbrevID -= (uint32_t)info.abbrevs.size();

  assert(!blockStack.empty());
  assert(abbrevID < blockStack.back().abbrevs.size());

  return blockStack.back().abbrevs[abbrevID];
}
