// Host-side check of dxvk_bc_decode.cpp against hand-computed blocks.
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "dxvk_bc_decode.cpp"

using namespace dxvk::bc;

static int failures = 0;

#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALLO: %s\n", msg); failures++; } } while (0)

struct Px { int r, g, b, a; };

static Px px(const uint8_t* img, int w, int x, int y) {
  const uint8_t* p = img + (y * w + x) * 4;
  return { p[0], p[1], p[2], p[3] };
}

static bool eq(Px p, int r, int g, int b, int a) { return p.r == r && p.g == g && p.b == b && p.a == a; }

int main() {
  uint8_t out[64 * 4];

  // BC1, c0 = red (0xF800), c1 = green (0x07E0), c0 > c1: 4 colors
  {
    uint8_t blk[8] = { 0x00, 0xF8, 0xE0, 0x07, 0, 0, 0, 0 };
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, blk, 8, 0, out, 4, 4, 1);
    CHECK(eq(px(out, 4, 0, 0), 255, 0, 0, 255), "BC1 indice 0 = rojo");

    uint8_t b1[8] = { 0x00, 0xF8, 0xE0, 0x07, 0x55, 0x55, 0x55, 0x55 };
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, b1, 8, 0, out, 4, 4, 1);
    CHECK(eq(px(out, 4, 3, 3), 0, 255, 0, 255), "BC1 indice 1 = verde");

    uint8_t b2[8] = { 0x00, 0xF8, 0xE0, 0x07, 0xAA, 0xAA, 0xAA, 0xAA };
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, b2, 8, 0, out, 4, 4, 1);
    CHECK(eq(px(out, 4, 1, 2), 170, 85, 0, 255), "BC1 indice 2 = 2/3 rojo + 1/3 verde");

    uint8_t b3[8] = { 0x00, 0xF8, 0xE0, 0x07, 0xFF, 0xFF, 0xFF, 0xFF };
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, b3, 8, 0, out, 4, 4, 1);
    CHECK(eq(px(out, 4, 2, 1), 85, 170, 0, 255), "BC1 indice 3 = 1/3 rojo + 2/3 verde");
  }

  // BC1, c0 < c1: 3 colors + transparent
  {
    uint8_t blk[8] = { 0xE0, 0x07, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF };
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, blk, 8, 0, out, 4, 4, 1);
    CHECK(eq(px(out, 4, 0, 0), 0, 0, 0, 0), "BC1 modo alfa: indice 3 transparente");

    uint8_t b2[8] = { 0xE0, 0x07, 0x00, 0xF8, 0xAA, 0xAA, 0xAA, 0xAA };
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, b2, 8, 0, out, 4, 4, 1);
    CHECK(eq(px(out, 4, 0, 0), 127, 127, 0, 255), "BC1 modo alfa: indice 2 = media");
  }

  // BC2: explicit alpha nibbles; colors always 4-color mode even if c0 <= c1
  {
    uint8_t blk[16] = { 0xF8, 0x08, 0, 0, 0, 0, 0, 0,   0xE0, 0x07, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF };
    decode(VK_FORMAT_BC2_UNORM_BLOCK, blk, 16, 0, out, 4, 4, 1);
    // bytes 0xF8, 0x08: texel0 = 0x8, texel1 = 0xF, texel2 = 0x8, texel3 = 0x0
    CHECK(px(out, 4, 0, 0).a == 136, "BC2 texel 0 nibble 0x8 = 136");
    CHECK(px(out, 4, 1, 0).a == 255, "BC2 texel 1 nibble 0xF = 255");
    CHECK(px(out, 4, 2, 0).a == 136, "BC2 texel 2 nibble 0x8 = 136");
    CHECK(px(out, 4, 3, 0).a == 0,   "BC2 texel 3 nibble 0x0 = 0");
    // c0 = green, c1 = red with c0 < c1; index 3 = (c0 + 2*c1) / 3 = (170, 85, 0) opaque in 4-color mode
    CHECK(px(out, 4, 0, 0).r == 170 && px(out, 4, 0, 0).g == 85 && px(out, 4, 0, 0).b == 0, "BC2 indice 3 en modo 4 colores");
  }

  // BC3 alpha interpolation, a0 > a1
  {
    uint8_t blk[16] = { 255, 0, 0x88, 0xC6, 0xFA, 0x00, 0x00, 0x00,   0x00, 0xF8, 0xE0, 0x07, 0, 0, 0, 0 };
    // alpha indices (3 bits each, little endian): 0,1,2,3,4,5,6,7 for the first 8 texels
    decode(VK_FORMAT_BC3_UNORM_BLOCK, blk, 16, 0, out, 4, 4, 1);
    CHECK(px(out, 4, 0, 0).a == 255, "BC3 alfa indice 0 = a0");
    CHECK(px(out, 4, 1, 0).a == 0,   "BC3 alfa indice 1 = a1");
    CHECK(px(out, 4, 2, 0).a == 218, "BC3 alfa indice 2 = 6/7 a0");
    CHECK(px(out, 4, 3, 0).a == 182, "BC3 alfa indice 3 = 5/7 a0");
    CHECK(px(out, 4, 0, 1).a == 145, "BC3 alfa indice 4 = 4/7 a0");
    CHECK(px(out, 4, 1, 1).a == 109, "BC3 alfa indice 5 = 3/7 a0");
    CHECK(px(out, 4, 2, 1).a == 72,  "BC3 alfa indice 6 = 2/7 a0");
    CHECK(px(out, 4, 3, 1).a == 36,  "BC3 alfa indice 7 = 1/7 a0");
  }

  // BC3 alpha, a0 <= a1: 0 and 255 endpoints at indices 6 and 7
  {
    uint8_t blk[16] = { 0, 255, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00,   0, 0, 0, 0, 0, 0, 0, 0 };
    // indices: 0,6,7 packed in the first 9 bits: 000 110 111 -> 0b111110000 = 0x1F0 ... build manually
    uint64_t bits = 0; const int idx[3] = { 0, 6, 7 };
    for (int i = 0; i < 3; i++) bits |= uint64_t(idx[i]) << (3 * i);
    for (int i = 0; i < 6; i++) blk[2 + i] = uint8_t(bits >> (8 * i));
    decode(VK_FORMAT_BC3_UNORM_BLOCK, blk, 16, 0, out, 4, 4, 1);
    CHECK(px(out, 4, 0, 0).a == 0,   "BC3 a0<=a1 indice 0 = a0 (0)");
    CHECK(px(out, 4, 1, 0).a == 0,   "BC3 a0<=a1 indice 6 = 0");
    CHECK(px(out, 4, 2, 0).a == 255, "BC3 a0<=a1 indice 7 = 255");
  }

  // BC4 unorm -> R8, 1 byte per texel
  {
    uint8_t blk[8] = { 200, 100, 0, 0, 0, 0, 0, 0 };
    uint8_t r[16];
    decode(VK_FORMAT_BC4_UNORM_BLOCK, blk, 8, 0, r, 4, 4, 1);
    CHECK(r[0] == 200 && r[15] == 200, "BC4 todo indice 0 = a0");
  }

  // BC5 unorm -> RG8
  {
    uint8_t blk[16] = { 10, 0, 0, 0, 0, 0, 0, 0,   20, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t rg[32];
    decode(VK_FORMAT_BC5_UNORM_BLOCK, blk, 16, 0, rg, 4, 4, 1);
    CHECK(rg[0] == 10 && rg[1] == 20 && rg[30] == 10 && rg[31] == 20, "BC5 canales R y G");
  }

  // BC4 snorm: endpoints -100 and 100 (two's complement bytes), index 0 -> -100
  {
    uint8_t blk[8] = { uint8_t(int8_t(-100)), 100, 0, 0, 0, 0, 0, 0 };
    uint8_t r[16];
    decode(VK_FORMAT_BC4_SNORM_BLOCK, blk, 8, 0, r, 4, 4, 1);
    CHECK(int8_t(r[0]) == -100, "BC4 snorm indice 0 = a0 (-100)");
  }

  // Cropping: a 2x2 level is stored as one full block; only 2x2 texels are written
  {
    uint8_t blk[8] = { 0x00, 0xF8, 0xE0, 0x07, 0x00, 0x00, 0x00, 0x00 };
    uint8_t small[2 * 2 * 4 + 16];
    std::memset(small, 0xCD, sizeof(small));
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, blk, 8, 0, small, 2, 2, 1);
    CHECK(small[16] == 0xCD, "recorte 2x2: no escribe mas alla del nivel");
    CHECK(eq(px(small, 2, 1, 1), 255, 0, 0, 255), "recorte 2x2: contenido correcto");
  }

  // Multi-block layout with row pitch larger than the data (8 wide, 2 block rows, 16 byte pitch)
  {
    uint8_t data[32];
    std::memset(data, 0, sizeof(data));
    const uint8_t red[8]  = { 0x00, 0xF8, 0xE0, 0x07, 0, 0, 0, 0 };
    const uint8_t blue[8] = { 0x1F, 0x00, 0xE0, 0x07, 0, 0, 0, 0 };
    std::memcpy(data + 0, red, 8);  std::memcpy(data + 8, blue, 8);
    std::memcpy(data + 16, blue, 8); std::memcpy(data + 24, red, 8);
    uint8_t img[8 * 8 * 4];
    decode(VK_FORMAT_BC1_RGBA_UNORM_BLOCK, data, 16, 0, img, 8, 8, 1);
    CHECK(eq(px(img, 8, 0, 0), 255, 0, 0, 255),  "bloque (0,0) rojo");
    CHECK(eq(px(img, 8, 4, 0), 0, 0, 255, 255),  "bloque (1,0) azul");
    CHECK(eq(px(img, 8, 0, 4), 0, 0, 255, 255),  "bloque (0,1) azul");
    CHECK(eq(px(img, 8, 7, 7), 255, 0, 0, 255),  "bloque (1,1) rojo");
  }

  std::printf(failures ? "\n%d fallos\n" : "\nTodas las pruebas pasaron\n", failures);
  return failures ? 1 : 0;
}
