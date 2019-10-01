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

#include <stdint.h>

namespace DXIL
{
enum class Features : uint64_t
{
  Double_precision_floating_point = 1 << 0,
  Raw_and_Structured_buffers = 1 << 1,
  UAVs_at_every_shader_stage = 1 << 2,
  _64_UAV_slots = 1 << 3,
  Minimum_precision_data_types = 1 << 4,
  Double_precision_extensions_for_11_1 = 1 << 5,
  Shader_extensions_for_11_1 = 1 << 6,
  Comparison_filtering_for_feature_level_9 = 1 << 7,
  Tiled_resources = 1 << 8,
  PS_Output_Stencil_Ref = 1 << 9,
  PS_Inner_Coverage = 1 << 10,
  Typed_UAV_Load_Additional_Formats = 1 << 11,
  Raster_Ordered_UAVs = 1 << 12,
  MultiView_From_Any_Shader = 1 << 13,
  Wave_level_operations = 1 << 14,
  _64_Bit_integer = 1 << 15,
  View_Instancing = 1 << 16,
  Barycentrics = 1 << 17,
  Use_native_low_precision = 1 << 18,
  Shading_Rate = 1 << 19,
  Raytracing_tier_1_1_features = 1 << 20,
  Sampler_feedback = 1 << 21,
};

class Program
{
public:
  Program(const void *bytes, size_t length);

private:
};

struct DebugName
{
  DebugName(const void *bytes, size_t length);

  uint16_t flags;
  const char *name;
};

};    // namespace DXIL
