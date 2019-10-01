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

#include <stdint.h>
#include <stdio.h>
#include <vector>

struct DXBCFileHeader
{
  uint32_t fourcc;          // "DXBC"
  uint32_t hashValue[4];    // unknown hash function and data
  uint32_t unknown;
  uint32_t fileLength;
  uint32_t numChunks;
  // uint32 chunkOffsets[numChunks]; follows
};

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

  std::vector<char> buffer;
  buffer.resize((size_t)size);

  size_t numRead = fread(&buffer[0], 1, buffer.size(), f);

  fclose(f);

  if(numRead != buffer.size())
  {
    fprintf(stderr, "Couldn't fully read file %s: %i\n", argv[1], errno);
    return 2;
  }

  return 0;
}