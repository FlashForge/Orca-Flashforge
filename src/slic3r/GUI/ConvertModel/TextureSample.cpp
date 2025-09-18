#include "TextureSample.hpp"

namespace Slic3r { namespace GUI {

TextureSample::TextureSample(const in_texture_data_t &textrue)
{
    m_bits = textrue.bits;
    m_pixelX = textrue.width;
    m_pixelY = textrue.height;
    m_channels = textrue.channels;
    m_bytesPerLine = textrue.bytePerLine;
    m_wraps[0] = textrue.wraps[0];
    m_wraps[1] = textrue.wraps[1];
    m_filpY = textrue.flipY;
}

cvt_color_t TextureSample::getTextureColor(float coordX, float coordY) const
{
    int x, y;
    getSampleSrc(m_pixelX * coordX, m_pixelX, m_wraps[0], x);
    getSampleSrc(m_pixelY * coordY, m_pixelY, m_wraps[1], y);

    const uint8_t *ptr;
    if (m_filpY) {
        ptr = m_bits + (m_pixelY - y - 1) * m_bytesPerLine + x * m_channels;
    } else {
        ptr = m_bits + y * m_bytesPerLine + x * m_channels;
    }
    cvt_color_t color;
    color[0] = ptr[0];
    color[1] = ptr[1];
    color[2] = ptr[2];
    return color;
}

void TextureSample::getSampleSrc(float coord, int size, texture_wrap_type_t wrap, int &p) const
{
    p = coord - 0.5f;
    if (wrap == TEX_WRAP_CLAMP_TO_EDGE) {
        uniformClampToEdgePos(p, size);
    } else if (wrap == TEX_WRAP_REPEAT) {
        uniformRepeatPos(p, size);
    } else {
        uniformMirroredRepeatPos(p, size);
    }
}

void TextureSample::uniformClampToEdgePos(int &p, int size) const
{
    if (p < 0) {
        p = 0;
    } else if (p >= size) {
        p = size - 1;
    }
}

void TextureSample::uniformRepeatPos(int &p, int size) const
{
    if (p < 0) {
        p += (-p + size - 1) / size * size;
    } else if (p >= size) {
        p -= p / size * size;
    }
}

void TextureSample::uniformMirroredRepeatPos(int &p, int size) const
{
    int repeatCnt;
    if (p < 0) {
        repeatCnt = (-p + size - 1) / size;
        p += repeatCnt * size;
    } else {
        repeatCnt = p / size;
        p -= repeatCnt * size;
    }
    if (repeatCnt % 2 != 0) {
        p = size - 1 - p;
    }
}

}} // namespace Slic3r::GUI
