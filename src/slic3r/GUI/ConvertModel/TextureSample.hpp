#ifndef slic3r_GUI_TextureSample_hpp_
#define slic3r_GUI_TextureSample_hpp_

#include <cstdint>
#include "ConvertModelDef.hpp"

namespace Slic3r { namespace GUI {

class TextureSample
{
public:
    TextureSample(const in_texture_data_t &textrue);

    cvt_color_t getTextureColor(float x, float y) const;

private:
    void getSampleSrc(float coord, int size, texture_wrap_type_t wrap, int &p) const;

    void uniformClampToEdgePos(int &p, int size) const;

    void uniformRepeatPos(int &p, int size) const;

    void uniformMirroredRepeatPos(int &p, int size) const;

private:
    const uint8_t *m_bits;
    int m_pixelX;
    int m_pixelY;
    int m_channels;
    int m_bytesPerLine;
    texture_wrap_type_t m_wraps[2];
    bool m_filpY;
};

}} // namespace Slic3r::GUI

#endif
