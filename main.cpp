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

#include <stdio.h>
#include <vector>
#include "common.h"
#include "dxil_inspect.h"

struct DXBCFileHeader
{
  uint32_t fourcc;          // "DXBC"
  uint8_t hashValue[16];    // unknown hash function and data
  uint16_t majorVersion;
  uint16_t minorVersion;
  uint32_t fileLength;
  uint32_t numChunks;
  // uint32 chunkOffsets[numChunks]; follows
};

struct DXBCChunkHeader
{
  uint32_t fourcc;
  uint32_t dataLength;
  // byte data[dataLength]; follows
};

DXIL::DebugName *debug_name = NULL;
DXIL::Features features;

int main(int argc, char **argv)
{
  if(argc != 2)
  {
    fprintf(stderr, "Usage: %s [file.dxbc]\n", argv[0]);
    return 1;
  }

  FILE *f = fopen(argv[1], "rb");
  if(f == NULL)
  {
    fprintf(stderr, "Couldn't open file %s: %i\n", argv[1], errno);
    return 2;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  std::vector<byte> buffer;
  buffer.resize((size_t)size);

  size_t numRead = fread(&buffer[0], 1, buffer.size(), f);

  fclose(f);

  if(numRead != buffer.size())
  {
    fprintf(stderr, "Couldn't fully read file %s: %i\n", argv[1], errno);
    return 2;
  }

  const byte *ptr = buffer.data();
  const DXBCFileHeader *header = (const DXBCFileHeader *)ptr;

  if(buffer.size() < sizeof(*header) || header->fileLength != buffer.size() ||
     header->fourcc != MAKE_FOURCC('D', 'X', 'B', 'C'))
  {
    fprintf(stderr, "Invalid DXBC file\n");
    return 3;
  }

  const uint32_t *offsets = (const uint32_t *)(header + 1);

  const DXBCChunkHeader *best_dxil_chunk = NULL;

  DXIL::Program *dxil = NULL;

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    const DXBCChunkHeader *chunk = (const DXBCChunkHeader *)(ptr + offsets[chunkIdx]);

    if(chunk->fourcc == MAKE_FOURCC('D', 'X', 'I', 'L'))
    {
      // only use DXIL if we don't already have debug DXIL
      if(!best_dxil_chunk)
        best_dxil_chunk = chunk;
    }
    else if(chunk->fourcc == MAKE_FOURCC('S', 'F', 'I', '0'))
    {
      features = *(DXIL::Features *)(chunk + 1);
    }
    else if(chunk->fourcc == MAKE_FOURCC('I', 'L', 'D', 'N'))
    {
      debug_name = new DXIL::DebugName(chunk + 1, chunk->dataLength);
    }
    else if(chunk->fourcc == MAKE_FOURCC('I', 'L', 'D', 'B'))
    {
      // debug DXIL is always the best
      best_dxil_chunk = chunk;
    }
  }

  if(best_dxil_chunk)
    dxil = new DXIL::Program(best_dxil_chunk + 1, best_dxil_chunk->dataLength);

  if(!dxil)
  {
    fprintf(stderr, "Couldn't find DXIL chunk\n");
    return 4;
  }

  return 0;
}