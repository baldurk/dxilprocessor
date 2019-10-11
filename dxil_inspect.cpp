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
#include <ctype.h>
#include <stdio.h>
#include <string>
#include "common.h"
#include "llvm_decoder.h"

// hack so we can dump everything together
extern DXIL::DebugName *debug_name;
extern DXIL::Features features;

namespace DXIL
{
struct ProgramHeader
{
  uint16_t ProgramVersion;
  uint16_t ProgramType;
  uint32_t SizeInUint32;     // Size in uint32_t units including this header.
  uint32_t DxilMagic;        // 0x4C495844, ASCII "DXIL".
  uint32_t DxilVersion;      // DXIL version.
  uint32_t BitcodeOffset;    // Offset to LLVM bitcode (from DxilMagic).
  uint32_t BitcodeSize;      // Size of LLVM bitcode.
};

enum class KnownBlocks : uint32_t
{
  BLOCKINFO = 0,

  // 1-7 reserved,

  MODULE_BLOCK = 8,
  PARAMATTR_BLOCK = 9,
  PARAMATTR_GROUP_BLOCK = 10,
  CONSTANTS_BLOCK = 11,
  FUNCTION_BLOCK = 12,
  TYPE_SYMTAB_BLOCK = 13,
  VALUE_SYMTAB_BLOCK = 14,
  METADATA_BLOCK = 15,
  METADATA_ATTACHMENT = 16,
  TYPE_BLOCK = 17,
};

enum class ModuleRecord : uint32_t
{
  VERSION = 1,
  TRIPLE = 2,
  DATALAYOUT = 3,
  FUNCTION = 8,
};

enum class ConstantsRecord : uint32_t
{
  SETTYPE = 1,
  CONST_NULL = 2,
  UNDEF = 3,
  INTEGER = 4,
  WIDE_INTEGER = 5,
  FLOAT = 6,
  AGGREGATE = 7,
  STRING = 8,
  DATA = 22,
};

enum class FunctionRecord : uint32_t
{
  DECLAREBLOCKS = 1,
  INST_BINOP = 2,
  INST_CAST = 3,
  INST_GEP_OLD = 4,
  INST_SELECT = 5,
  INST_EXTRACTELT = 6,
  INST_INSERTELT = 7,
  INST_SHUFFLEVEC = 8,
  INST_CMP = 9,
  INST_RET = 10,
  INST_BR = 11,
  INST_SWITCH = 12,
  INST_INVOKE = 13,
  INST_UNREACHABLE = 15,
  INST_PHI = 16,
  INST_ALLOCA = 19,
  INST_LOAD = 20,
  INST_VAARG = 23,
  INST_STORE_OLD = 24,
  INST_EXTRACTVAL = 26,
  INST_INSERTVAL = 27,
  INST_CMP2 = 28,
  INST_VSELECT = 29,
  INST_INBOUNDS_GEP_OLD = 30,
  INST_INDIRECTBR = 31,
  DEBUG_LOC_AGAIN = 33,
  INST_CALL = 34,
  DEBUG_LOC = 35,
  INST_FENCE = 36,
  INST_CMPXCHG_OLD = 37,
  INST_ATOMICRMW = 38,
  INST_RESUME = 39,
  INST_LANDINGPAD_OLD = 40,
  INST_LOADATOMIC = 41,
  INST_STOREATOMIC_OLD = 42,
  INST_GEP = 43,
  INST_STORE = 44,
  INST_STOREATOMIC = 45,
  INST_CMPXCHG = 46,
  INST_LANDINGPAD = 47,
  INST_CLEANUPRET = 48,
  INST_CATCHRET = 49,
  INST_CATCHPAD = 50,
  INST_CLEANUPPAD = 51,
  INST_CATCHSWITCH = 52,
  OPERAND_BUNDLE = 55,
  INST_UNOP = 56,
  INST_CALLBR = 57,
};

enum class ValueSymtabRecord : uint32_t
{
  ENTRY = 1,
  BBENTRY = 2,
  FNENTRY = 3,
  COMBINED_ENTRY = 5,
};

enum class MetaDataRecord : uint32_t
{
  STRING_OLD = 1,
  VALUE = 2,
  NODE = 3,
  NAME = 4,
  DISTINCT_NODE = 5,
  KIND = 6,
  LOCATION = 7,
  OLD_NODE = 8,
  OLD_FN_NODE = 9,
  NAMED_NODE = 10,
  ATTACHMENT = 11,
  GENERIC_DEBUG = 12,
  SUBRANGE = 13,
  ENUMERATOR = 14,
  BASIC_TYPE = 15,
  FILE = 16,
  DERIVED_TYPE = 17,
  COMPOSITE_TYPE = 18,
  SUBROUTINE_TYPE = 19,
  COMPILE_UNIT = 20,
  SUBPROGRAM = 21,
  LEXICAL_BLOCK = 22,
  LEXICAL_BLOCK_FILE = 23,
  NAMESPACE = 24,
  TEMPLATE_TYPE = 25,
  TEMPLATE_VALUE = 26,
  GLOBAL_VAR = 27,
  LOCAL_VAR = 28,
  EXPRESSION = 29,
  OBJC_PROPERTY = 30,
  IMPORTED_ENTITY = 31,
  MODULE = 32,
  MACRO = 33,
  MACRO_FILE = 34,
  STRINGS = 35,
  GLOBAL_DECL_ATTACHMENT = 36,
  GLOBAL_VAR_EXPR = 37,
  INDEX_OFFSET = 38,
  INDEX = 39,
  LABEL = 40,
  COMMON_BLOCK = 44,
};

enum class TypeRecord : uint32_t
{
  NUMENTRY = 1,
  VOID = 2,
  FLOAT = 3,
  DOUBLE = 4,
  LABEL = 5,
  OPAQUE = 6,
  INTEGER = 7,
  POINTER = 8,
  FUNCTION_OLD = 9,
  HALF = 10,
  ARRAY = 11,
  VECTOR = 12,
  METADATA = 16,
  STRUCT_ANON = 18,
  STRUCT_NAME = 19,
  STRUCT_NAMED = 20,
  FUNCTION = 21,
  TOKEN = 22,
};

static void printName(uint32_t parentBlock, const LLVMBC::BlockOrRecord &block)
{
  const char *name = NULL;

  if(block.IsBlock())
  {
    // GetBlockName in BitcodeAnalyzer.cpp
    switch(KnownBlocks(block.id))
    {
      case KnownBlocks::BLOCKINFO: name = "BLOCKINFO"; break;
      case KnownBlocks::MODULE_BLOCK: name = "MODULE_BLOCK"; break;
      case KnownBlocks::PARAMATTR_BLOCK: name = "PARAMATTR_BLOCK"; break;
      case KnownBlocks::PARAMATTR_GROUP_BLOCK: name = "PARAMATTR_GROUP_BLOCK"; break;
      case KnownBlocks::CONSTANTS_BLOCK: name = "CONSTANTS_BLOCK"; break;
      case KnownBlocks::FUNCTION_BLOCK: name = "FUNCTION_BLOCK"; break;
      case KnownBlocks::TYPE_SYMTAB_BLOCK: name = "TYPE_SYMTAB_BLOCK"; break;
      case KnownBlocks::VALUE_SYMTAB_BLOCK: name = "VALUE_SYMTAB_BLOCK"; break;
      case KnownBlocks::METADATA_BLOCK: name = "METADATA_BLOCK"; break;
      case KnownBlocks::METADATA_ATTACHMENT: name = "METADATA_ATTACHMENT"; break;
      case KnownBlocks::TYPE_BLOCK: name = "TYPE_BLOCK"; break;
      default: break;
    }
  }
  else
  {
#define STRINGISE_RECORD(a) \
  case decltype(code)::a: name = #a; break;

    // GetCodeName in BitcodeAnalyzer.cpp
    switch(KnownBlocks(parentBlock))
    {
      case KnownBlocks::MODULE_BLOCK:
      {
        ModuleRecord code = ModuleRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(VERSION);
          STRINGISE_RECORD(TRIPLE);
          STRINGISE_RECORD(DATALAYOUT);
          STRINGISE_RECORD(FUNCTION);
          default: break;
        }
        break;
      }
      case KnownBlocks::PARAMATTR_BLOCK:
      case KnownBlocks::PARAMATTR_GROUP_BLOCK: name = "ENTRY"; break;
      case KnownBlocks::CONSTANTS_BLOCK:
      {
        ConstantsRecord code = ConstantsRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(SETTYPE);
          STRINGISE_RECORD(UNDEF);
          STRINGISE_RECORD(INTEGER);
          STRINGISE_RECORD(WIDE_INTEGER);
          STRINGISE_RECORD(FLOAT);
          STRINGISE_RECORD(AGGREGATE);
          STRINGISE_RECORD(STRING);
          STRINGISE_RECORD(DATA);
          case ConstantsRecord::CONST_NULL: name = "NULL"; break;
          default: break;
        }
        break;
      }
      case KnownBlocks::FUNCTION_BLOCK:
      {
        FunctionRecord code = FunctionRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(DECLAREBLOCKS);
          STRINGISE_RECORD(INST_BINOP);
          STRINGISE_RECORD(INST_CAST);
          STRINGISE_RECORD(INST_GEP_OLD);
          STRINGISE_RECORD(INST_SELECT);
          STRINGISE_RECORD(INST_EXTRACTELT);
          STRINGISE_RECORD(INST_INSERTELT);
          STRINGISE_RECORD(INST_SHUFFLEVEC);
          STRINGISE_RECORD(INST_CMP);
          STRINGISE_RECORD(INST_RET);
          STRINGISE_RECORD(INST_BR);
          STRINGISE_RECORD(INST_SWITCH);
          STRINGISE_RECORD(INST_INVOKE);
          STRINGISE_RECORD(INST_UNREACHABLE);
          STRINGISE_RECORD(INST_PHI);
          STRINGISE_RECORD(INST_ALLOCA);
          STRINGISE_RECORD(INST_LOAD);
          STRINGISE_RECORD(INST_VAARG);
          STRINGISE_RECORD(INST_STORE_OLD);
          STRINGISE_RECORD(INST_EXTRACTVAL);
          STRINGISE_RECORD(INST_INSERTVAL);
          STRINGISE_RECORD(INST_CMP2);
          STRINGISE_RECORD(INST_VSELECT);
          STRINGISE_RECORD(INST_INBOUNDS_GEP_OLD);
          STRINGISE_RECORD(INST_INDIRECTBR);
          STRINGISE_RECORD(DEBUG_LOC_AGAIN);
          STRINGISE_RECORD(INST_CALL);
          STRINGISE_RECORD(DEBUG_LOC);
          STRINGISE_RECORD(INST_FENCE);
          STRINGISE_RECORD(INST_CMPXCHG_OLD);
          STRINGISE_RECORD(INST_ATOMICRMW);
          STRINGISE_RECORD(INST_RESUME);
          STRINGISE_RECORD(INST_LANDINGPAD_OLD);
          STRINGISE_RECORD(INST_LOADATOMIC);
          STRINGISE_RECORD(INST_STOREATOMIC_OLD);
          STRINGISE_RECORD(INST_GEP);
          STRINGISE_RECORD(INST_STORE);
          STRINGISE_RECORD(INST_STOREATOMIC);
          STRINGISE_RECORD(INST_CMPXCHG);
          STRINGISE_RECORD(INST_LANDINGPAD);
          STRINGISE_RECORD(INST_CLEANUPRET);
          STRINGISE_RECORD(INST_CATCHRET);
          STRINGISE_RECORD(INST_CATCHPAD);
          STRINGISE_RECORD(INST_CLEANUPPAD);
          STRINGISE_RECORD(INST_CATCHSWITCH);
          STRINGISE_RECORD(OPERAND_BUNDLE);
          STRINGISE_RECORD(INST_UNOP);
          STRINGISE_RECORD(INST_CALLBR);
          default: break;
        }
        break;
      }
      case KnownBlocks::VALUE_SYMTAB_BLOCK:
      {
        ValueSymtabRecord code = ValueSymtabRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(ENTRY);
          STRINGISE_RECORD(BBENTRY);
          STRINGISE_RECORD(FNENTRY);
          STRINGISE_RECORD(COMBINED_ENTRY);
          default: break;
        }
        break;
      }
      case KnownBlocks::METADATA_BLOCK:
      {
        MetaDataRecord code = MetaDataRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(STRING_OLD);
          STRINGISE_RECORD(VALUE);
          STRINGISE_RECORD(NODE);
          STRINGISE_RECORD(NAME);
          STRINGISE_RECORD(DISTINCT_NODE);
          STRINGISE_RECORD(KIND);
          STRINGISE_RECORD(LOCATION);
          STRINGISE_RECORD(OLD_NODE);
          STRINGISE_RECORD(OLD_FN_NODE);
          STRINGISE_RECORD(NAMED_NODE);
          STRINGISE_RECORD(ATTACHMENT);
          STRINGISE_RECORD(GENERIC_DEBUG);
          STRINGISE_RECORD(SUBRANGE);
          STRINGISE_RECORD(ENUMERATOR);
          STRINGISE_RECORD(BASIC_TYPE);
          STRINGISE_RECORD(FILE);
          STRINGISE_RECORD(DERIVED_TYPE);
          STRINGISE_RECORD(COMPOSITE_TYPE);
          STRINGISE_RECORD(SUBROUTINE_TYPE);
          STRINGISE_RECORD(COMPILE_UNIT);
          STRINGISE_RECORD(SUBPROGRAM);
          STRINGISE_RECORD(LEXICAL_BLOCK);
          STRINGISE_RECORD(LEXICAL_BLOCK_FILE);
          STRINGISE_RECORD(NAMESPACE);
          STRINGISE_RECORD(TEMPLATE_TYPE);
          STRINGISE_RECORD(TEMPLATE_VALUE);
          STRINGISE_RECORD(GLOBAL_VAR);
          STRINGISE_RECORD(LOCAL_VAR);
          STRINGISE_RECORD(EXPRESSION);
          STRINGISE_RECORD(OBJC_PROPERTY);
          STRINGISE_RECORD(IMPORTED_ENTITY);
          STRINGISE_RECORD(MODULE);
          STRINGISE_RECORD(MACRO);
          STRINGISE_RECORD(MACRO_FILE);
          STRINGISE_RECORD(STRINGS);
          STRINGISE_RECORD(GLOBAL_DECL_ATTACHMENT);
          STRINGISE_RECORD(GLOBAL_VAR_EXPR);
          STRINGISE_RECORD(INDEX_OFFSET);
          STRINGISE_RECORD(INDEX);
          STRINGISE_RECORD(LABEL);
          STRINGISE_RECORD(COMMON_BLOCK);
          default: break;
        }
        break;
      }
      case KnownBlocks::TYPE_BLOCK:
      {
        TypeRecord code = TypeRecord(block.id);
        switch(code)
        {
          STRINGISE_RECORD(NUMENTRY);
          STRINGISE_RECORD(VOID);
          STRINGISE_RECORD(FLOAT);
          STRINGISE_RECORD(DOUBLE);
          STRINGISE_RECORD(LABEL);
          STRINGISE_RECORD(OPAQUE);
          STRINGISE_RECORD(INTEGER);
          STRINGISE_RECORD(POINTER);
          STRINGISE_RECORD(FUNCTION_OLD);
          STRINGISE_RECORD(HALF);
          STRINGISE_RECORD(ARRAY);
          STRINGISE_RECORD(VECTOR);
          STRINGISE_RECORD(METADATA);
          STRINGISE_RECORD(STRUCT_ANON);
          STRINGISE_RECORD(STRUCT_NAME);
          STRINGISE_RECORD(STRUCT_NAMED);
          STRINGISE_RECORD(FUNCTION);
          STRINGISE_RECORD(TOKEN);
          default: break;
        }
        break;
      }
    }
  }

  // fallback
  if(name)
  {
    printf("%s", name);
  }
  else
  {
    if(block.IsBlock())
      printf("BLOCK%d", block.id);
    else
      printf("RECORD%d", block.id);
  }
}

static void dumpRecord(uint32_t parentBlock, const LLVMBC::BlockOrRecord &record, int indent)
{
  printf("%*s", indent, "");
  printf("<");
  printName(parentBlock, record);

  if(KnownBlocks(parentBlock) == KnownBlocks::METADATA_BLOCK &&
     (MetaDataRecord(record.id) == MetaDataRecord::STRING_OLD ||
      MetaDataRecord(record.id) == MetaDataRecord::NAME ||
      MetaDataRecord(record.id) == MetaDataRecord::KIND))
  {
    printf(" record string = '");
    for(size_t i = 0; i < record.ops.size(); i++)
    {
      if(record.ops[i] == '\'')
        printf("\\'");
      else if(record.ops[i] == '\\')
        printf("\\\\");
      else if(isprint(char(record.ops[i])))
        printf("%c", char(record.ops[i]));
      else
        printf("\\x%02x", (uint32_t)record.ops[i]);
    }
    printf("'");
  }
  else
  {
    for(size_t i = 0; i < record.ops.size(); i++)
      printf(" op%u=%llu", (uint32_t)i, record.ops[i]);
  }

  if(record.blob)
    printf(" with blob of %u bytes", (uint32_t)record.blobLength);

  printf("/>\n");
}

static void dumpBlock(const LLVMBC::BlockOrRecord &block, int indent)
{
  printf("%*s", indent, "");
  if(block.children.empty() || KnownBlocks(block.id) == KnownBlocks::BLOCKINFO)
  {
    printf("<");
    printName(0, block);
    printf("/>\n");
    return;
  }

  printf("<");
  printName(0, block);
  printf(" NumWords=%u>\n", block.blockDwordLength);

  for(const LLVMBC::BlockOrRecord &child : block.children)
  {
    if(child.IsBlock())
      dumpBlock(child, indent + 2);
    else
      dumpRecord(block.id, child, indent + 2);
  }

  printf("%*s", indent, "");
  printf("</");
  printName(0, block);
  printf(">\n");
}

static std::string getString(const std::vector<uint64_t> &ops, size_t i = 0)
{
  std::string ret;
  ret.reserve(ops.size());
  for(; i < ops.size(); i++)
  {
    uint64_t c = ops[i];
    if(c == '\'')
    {
      ret.push_back('\\');
      ret.push_back('\'');
    }
    else if(c == '\\')
    {
      ret.push_back('\\');
      ret.push_back('\\');
    }
    else if(c == '\r')
    {
      ret.push_back('\\');
      ret.push_back('r');
    }
    else if(c == '\n')
    {
      ret.push_back('\\');
      ret.push_back('n');
    }
    else if(c == '\t')
    {
      ret.push_back('\\');
      ret.push_back('t');
    }
    else if(isprint(char(c)))
    {
      ret.push_back(char(c));
    }
    else
    {
      ret.push_back('\\');
      ret.push_back('x');
      ret.push_back('.');
      ret.push_back('.');
    }
  }
  return ret;
}

Program::Program(const void *bytes, size_t length)
{
  const byte *ptr = (const byte *)bytes;
  const ProgramHeader *header = (const ProgramHeader *)ptr;
  assert(header->DxilMagic == MAKE_FOURCC('D', 'X', 'I', 'L'));

  const byte *bitcode = ((const byte *)&header->DxilMagic) + header->BitcodeOffset;
  assert(bitcode + header->BitcodeSize == ptr + length);

  LLVMBC::BitcodeReader reader(bitcode, header->BitcodeSize);

  LLVMBC::BlockOrRecord root = reader.ReadToplevelBlock();

  // the top-level block should be MODULE_BLOCK
  assert(KnownBlocks(root.id) == KnownBlocks::MODULE_BLOCK);

  // we should have consumed all bits, only one top-level block
  assert(reader.AtEndOfStream());

  const char *shaderName[] = {
      "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
      "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
      "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
  };

  printf("; %s Shader, compiled under SM%u.%u\n", shaderName[header->ProgramType],
         (header->ProgramVersion & 0xf0) >> 4, header->ProgramVersion & 0xf);

  if(debug_name)
    printf("; shader debug name: %s\n;\n", debug_name->name);

  // Input signature and Output signature haven't changed.
  // Pipeline Runtime Information we have decoded just not implemented here

  std::string datalayout, triple;

#define IS_KNOWN(val, KnownID) (decltype(KnownID)(val) == KnownID)

  const LLVMBC::BlockOrRecord *metadata = NULL;

  for(const LLVMBC::BlockOrRecord &rootblock : root.children)
  {
    if(rootblock.IsRecord() && IS_KNOWN(rootblock.id, ModuleRecord::TRIPLE))
    {
      printf("target triple = \"%s\"\n", getString(rootblock.ops).c_str());
    }
    else if(rootblock.IsRecord() && IS_KNOWN(rootblock.id, ModuleRecord::DATALAYOUT))
    {
      printf("target datalayout = \"%s\"\n", getString(rootblock.ops).c_str());
    }
    else if(rootblock.IsBlock() && IS_KNOWN(rootblock.id, KnownBlocks::VALUE_SYMTAB_BLOCK))
    {
      for(const LLVMBC::BlockOrRecord &symtab : rootblock.children)
      {
        printf("function %llu is \"%s\"\n", symtab.ops[0], getString(symtab.ops, 1).c_str());
      }
    }
    else if(rootblock.IsBlock() && IS_KNOWN(rootblock.id, KnownBlocks::METADATA_BLOCK))
    {
      for(size_t i = 0; i < rootblock.children.size(); i++)
      {
        const LLVMBC::BlockOrRecord &meta = rootblock.children[i];
        if(IS_KNOWN(meta.id, MetaDataRecord::NAME))
        {
          std::string metaName = getString(meta.ops);
          i++;
          const LLVMBC::BlockOrRecord &namedNode = rootblock.children[i];
          assert(IS_KNOWN(namedNode.id, MetaDataRecord::NAMED_NODE));

          printf("!%s = !{", metaName.c_str());
          bool first = true;
          for(uint64_t op : namedNode.ops)
          {
            if(!first)
              printf(", ");
            printf("%llu", op);
            first = false;
          }
          printf("}\n");
        }
        else
        {
          if(IS_KNOWN(meta.id, MetaDataRecord::KIND))
          {
            printf("Kind[%llu] = %s\n", meta.ops[0], getString(meta.ops, 1).c_str());
            continue;
          }

          printf("!%u = ", (uint32_t)i);

          auto getMetaString = [&rootblock](uint64_t id) -> std::string {
            return id ? getString(rootblock.children[id - 1].ops) : "NULL";
          };

          if(IS_KNOWN(meta.id, MetaDataRecord::STRING_OLD))
          {
            printf("\"%s\"", getString(meta.ops).c_str());
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::FILE))
          {
            if(meta.ops[0])
              printf("distinct ");

            printf("!DIFile(");
            printf("filename: \"%s\"", getMetaString(meta.ops[1]).c_str());
            printf(", directory: \"%s\"", getMetaString(meta.ops[2]).c_str());
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::NODE) ||
                  IS_KNOWN(meta.id, MetaDataRecord::DISTINCT_NODE))
          {
            if(IS_KNOWN(meta.id, MetaDataRecord::DISTINCT_NODE))
              printf("distinct ");

            printf("!{");
            bool first = true;
            for(uint64_t op : meta.ops)
            {
              if(!first)
                printf(", ");
              printf("!%llu", op - 1);
              first = false;
            }
            printf("}");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::BASIC_TYPE))
          {
            printf("!DIBasicType(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::DERIVED_TYPE))
          {
            printf("!DIDerivedType(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::COMPOSITE_TYPE))
          {
            printf("!DICompositeType(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::SUBROUTINE_TYPE))
          {
            printf("!DISubroutineType(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::TEMPLATE_TYPE))
          {
            printf("!DITemplateTypeParameter(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::TEMPLATE_VALUE))
          {
            printf("!DITemplateValueParameter(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::SUBPROGRAM))
          {
            printf("!DISubprogram(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::LOCATION))
          {
            printf("!DILocation(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::LOCAL_VAR))
          {
            printf("!DILocalVariable(");
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::VALUE))
          {
            // need to decode CONSTANTS_BLOCK and TYPE_BLOCK for this
            printf("!{values[%llu] interpreted as types[%llu]}", meta.ops[1], meta.ops[0]);
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::EXPRESSION))
          {
            // don't decode this yet
            printf("!DIExpression(");
            bool first = true;
            for(uint64_t op : meta.ops)
            {
              if(!first)
                printf(", ");
              printf("%llu", op);
              first = false;
            }
            printf(")");
          }
          else if(IS_KNOWN(meta.id, MetaDataRecord::COMPILE_UNIT))
          {
            // should be at least 14 parameters
            assert(meta.ops.size() >= 14);

            // we expect it to be marked as distinct, but we'll always treat it that way
            if(meta.ops[0])
              printf("distinct ");
            else
              printf("distinct? ");

            printf("!DICompileUnit(");
            {
              printf("language: %s",
                     meta.ops[1] == 0x4 ? "DW_LANG_C_plus_plus" : "DW_LANG_unknown");
              printf(", file: !%llu", meta.ops[2] - 1);
              printf(", producer: \"%s\"", getMetaString(meta.ops[3]).c_str());
              printf(", isOptimized: %s", meta.ops[4] ? "true" : "false");
              printf(", flags: \"%s\"", getMetaString(meta.ops[5]).c_str());
              printf(", runtimeVersion: %llu", meta.ops[6]);
              printf(", splitDebugFilename: \"%s\"", getMetaString(meta.ops[7]).c_str());
              printf(", emissionKind: %llu", meta.ops[8]);
              printf(", enums: !%llu", meta.ops[9] - 1);
              printf(", retainedTypes: !%llu", meta.ops[10] - 1);
              printf(", subprograms: !%llu", meta.ops[11] - 1);
              printf(", globals: !%llu", meta.ops[12] - 1);
              printf(", imports: !%llu", meta.ops[13] - 1);
              if(meta.ops.size() >= 15)
                printf(", dwoId: 0x%llu", meta.ops[14]);
            }
            printf(")");
          }
          else
          {
            assert(false && "unhandled metadata type");
          }

          printf("\n");
        }
      }
    }

    printf("\n");
  }

  dumpBlock(root, 0);
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