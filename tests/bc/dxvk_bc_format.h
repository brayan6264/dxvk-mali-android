// Minimal stand-in so dxvk_bc_decode.cpp builds on the host without DXVK's headers.
#pragma once

#include <cstdint>

using VkDeviceSize = uint64_t;

enum VkFormat {
  VK_FORMAT_UNDEFINED = 0,
  VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133,
  VK_FORMAT_BC1_RGBA_SRGB_BLOCK = 134,
  VK_FORMAT_BC2_UNORM_BLOCK = 135,
  VK_FORMAT_BC2_SRGB_BLOCK = 136,
  VK_FORMAT_BC3_UNORM_BLOCK = 137,
  VK_FORMAT_BC3_SRGB_BLOCK = 138,
  VK_FORMAT_BC4_UNORM_BLOCK = 139,
  VK_FORMAT_BC4_SNORM_BLOCK = 140,
  VK_FORMAT_BC5_UNORM_BLOCK = 141,
  VK_FORMAT_BC5_SNORM_BLOCK = 142,
};

namespace dxvk::bc {

  void decode(
          VkFormat      format,
    const void*         src,
          VkDeviceSize  srcRowPitch,
          VkDeviceSize  srcDepthPitch,
          void*         dst,
          uint32_t      width,
          uint32_t      height,
          uint32_t      depth);

}
