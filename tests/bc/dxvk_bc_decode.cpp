#include <algorithm>
#include <cstring>

#include "dxvk_bc_format.h"

namespace dxvk::bc {

  namespace {

    struct Rgba8 {
      uint8_t r, g, b, a;
    };

    inline uint8_t expand5(uint32_t v) { return uint8_t((v << 3) | (v >> 2)); }
    inline uint8_t expand6(uint32_t v) { return uint8_t((v << 2) | (v >> 4)); }


    // Decodes the 8 byte color block shared by BC1, BC2 and BC3. Only BC1
    // may use the 3-color + transparent mode, BC2/BC3 always use 4 colors.
    void decodeColorBlock(const uint8_t* b, Rgba8* out, bool allowTransparent) {
      const uint32_t c0 = uint32_t(b[0]) | (uint32_t(b[1]) << 8);
      const uint32_t c1 = uint32_t(b[2]) | (uint32_t(b[3]) << 8);

      Rgba8 p[4];
      p[0] = { expand5(c0 >> 11), expand6((c0 >> 5) & 0x3f), expand5(c0 & 0x1f), 255 };
      p[1] = { expand5(c1 >> 11), expand6((c1 >> 5) & 0x3f), expand5(c1 & 0x1f), 255 };

      if (c0 > c1 || !allowTransparent) {
        p[2] = { uint8_t((2 * p[0].r + p[1].r) / 3), uint8_t((2 * p[0].g + p[1].g) / 3), uint8_t((2 * p[0].b + p[1].b) / 3), 255 };
        p[3] = { uint8_t((p[0].r + 2 * p[1].r) / 3), uint8_t((p[0].g + 2 * p[1].g) / 3), uint8_t((p[0].b + 2 * p[1].b) / 3), 255 };
      } else {
        p[2] = { uint8_t((p[0].r + p[1].r) / 2), uint8_t((p[0].g + p[1].g) / 2), uint8_t((p[0].b + p[1].b) / 2), 255 };
        p[3] = { 0, 0, 0, 0 };
      }

      const uint32_t indices = uint32_t(b[4]) | (uint32_t(b[5]) << 8)
                             | (uint32_t(b[6]) << 16) | (uint32_t(b[7]) << 24);

      for (uint32_t i = 0; i < 16; i++)
        out[i] = p[(indices >> (2 * i)) & 3];
    }


    // Decodes the 8 byte interpolated block used for BC3 alpha, BC4 and BC5.
    void decodeUnormChannelBlock(const uint8_t* b, uint8_t* out) {
      uint32_t t[8];
      t[0] = b[0];
      t[1] = b[1];

      if (t[0] > t[1]) {
        for (uint32_t i = 1; i < 7; i++)
          t[1 + i] = ((7 - i) * t[0] + i * t[1]) / 7;
      } else {
        for (uint32_t i = 1; i < 5; i++)
          t[1 + i] = ((5 - i) * t[0] + i * t[1]) / 5;
        t[6] = 0;
        t[7] = 255;
      }

      uint64_t bits = 0;
      for (uint32_t i = 0; i < 6; i++)
        bits |= uint64_t(b[2 + i]) << (8 * i);

      for (uint32_t i = 0; i < 16; i++)
        out[i] = uint8_t(t[(bits >> (3 * i)) & 7]);
    }


    // Same as above for the signed BC4/BC5 variants. Output bytes hold
    // two's complement values in the -127..127 range.
    void decodeSnormChannelBlock(const uint8_t* b, uint8_t* out) {
      int32_t t[8];
      t[0] = std::max<int32_t>(int8_t(b[0]), -127);
      t[1] = std::max<int32_t>(int8_t(b[1]), -127);

      if (t[0] > t[1]) {
        for (int32_t i = 1; i < 7; i++)
          t[1 + i] = ((7 - i) * t[0] + i * t[1]) / 7;
      } else {
        for (int32_t i = 1; i < 5; i++)
          t[1 + i] = ((5 - i) * t[0] + i * t[1]) / 5;
        t[6] = -127;
        t[7] = 127;
      }

      uint64_t bits = 0;
      for (uint32_t i = 0; i < 6; i++)
        bits |= uint64_t(b[2 + i]) << (8 * i);

      for (uint32_t i = 0; i < 16; i++)
        out[i] = uint8_t(int8_t(t[(bits >> (3 * i)) & 7]));
    }


    struct BlockLayout {
      uint32_t blockBytes;
      uint32_t texelBytes;
    };


    BlockLayout getLayout(VkFormat format) {
      switch (format) {
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return { 8, 4 };
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:      return { 16, 4 };
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:     return { 8, 1 };
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:     return { 16, 2 };
        default:                            return { 0, 0 };
      }
    }


    // Decodes one block into 16 texels of texelBytes each, row-major.
    void decodeBlock(VkFormat format, const uint8_t* b, uint8_t* texels) {
      switch (format) {
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: {
          decodeColorBlock(b, reinterpret_cast<Rgba8*>(texels), true);
        } break;

        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK: {
          auto rgba = reinterpret_cast<Rgba8*>(texels);
          decodeColorBlock(b + 8, rgba, false);

          for (uint32_t i = 0; i < 16; i++) {
            uint32_t nibble = (b[i >> 1] >> ((i & 1) * 4)) & 0xf;
            rgba[i].a = uint8_t(nibble * 17);
          }
        } break;

        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK: {
          auto rgba = reinterpret_cast<Rgba8*>(texels);
          decodeColorBlock(b + 8, rgba, false);

          uint8_t alpha[16];
          decodeUnormChannelBlock(b, alpha);

          for (uint32_t i = 0; i < 16; i++)
            rgba[i].a = alpha[i];
        } break;

        case VK_FORMAT_BC4_UNORM_BLOCK:
          decodeUnormChannelBlock(b, texels);
          break;

        case VK_FORMAT_BC4_SNORM_BLOCK:
          decodeSnormChannelBlock(b, texels);
          break;

        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK: {
          uint8_t r[16], g[16];

          if (format == VK_FORMAT_BC5_UNORM_BLOCK) {
            decodeUnormChannelBlock(b,     r);
            decodeUnormChannelBlock(b + 8, g);
          } else {
            decodeSnormChannelBlock(b,     r);
            decodeSnormChannelBlock(b + 8, g);
          }

          for (uint32_t i = 0; i < 16; i++) {
            texels[2 * i + 0] = r[i];
            texels[2 * i + 1] = g[i];
          }
        } break;

        default:
          break;
      }
    }

  }


  void decode(
          VkFormat      format,
    const void*         src,
          VkDeviceSize  srcRowPitch,
          VkDeviceSize  srcDepthPitch,
          void*         dst,
          uint32_t      width,
          uint32_t      height,
          uint32_t      depth) {
    const BlockLayout layout = getLayout(format);

    if (!layout.blockBytes || !width || !height || !depth)
      return;

    const uint32_t blocksX = (width  + 3) / 4;
    const uint32_t blocksY = (height + 3) / 4;

    const VkDeviceSize dstRowPitch   = VkDeviceSize(width) * layout.texelBytes;
    const VkDeviceSize dstDepthPitch = VkDeviceSize(height) * dstRowPitch;

    auto srcBytes = reinterpret_cast<const uint8_t*>(src);
    auto dstBytes = reinterpret_cast<uint8_t*>(dst);

    uint8_t texels[16 * 4];

    for (uint32_t z = 0; z < depth; z++) {
      for (uint32_t by = 0; by < blocksY; by++) {
        const uint8_t* srcRow = srcBytes + z * srcDepthPitch + by * srcRowPitch;

        const uint32_t rows = std::min(4u, height - by * 4);

        for (uint32_t bx = 0; bx < blocksX; bx++) {
          decodeBlock(format, srcRow + bx * layout.blockBytes, texels);

          const uint32_t cols = std::min(4u, width - bx * 4);

          for (uint32_t y = 0; y < rows; y++) {
            uint8_t* dstTexel = dstBytes + z * dstDepthPitch
              + (by * 4 + y) * dstRowPitch + VkDeviceSize(bx * 4) * layout.texelBytes;

            std::memcpy(dstTexel, texels + y * 4 * layout.texelBytes, cols * layout.texelBytes);
          }
        }
      }
    }
  }

}
