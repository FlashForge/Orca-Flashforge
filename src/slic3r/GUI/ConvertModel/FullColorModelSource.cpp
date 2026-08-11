#include "FullColorModelSource.hpp"

#include <atomic>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace Slic3r { namespace GUI {

namespace {

std::atomic<uint64_t> next_source_id{ 1 };

bool fail_with_message(std::string *error_message, const char *message)
{
    if (error_message != nullptr)
        *error_message = message;
    return false;
}

template<typename T>
T read_value(const void *address)
{
    T value;
    std::memcpy(&value, address, sizeof(T));
    return value;
}

const uint8_t *offset_pointer(const void *base, size_t offset)
{
    return static_cast<const uint8_t *>(base) + offset;
}

} // namespace

bool FullColorTexture::valid() const
{
    if (width <= 0 || height <= 0 || channels < 3 || bytes_per_line < width * channels)
        return false;

    const size_t row_size  = static_cast<size_t>(bytes_per_line);
    const size_t row_count = static_cast<size_t>(height);
    return row_count <= std::numeric_limits<size_t>::max() / row_size && pixels.size() >= row_size * row_count;
}

bool FullColorModelSource::create_from_legacy_input(const in_model_data_t &input, FullColorModelSource &output,
    std::string *error_message)
{
    FullColorModelSource source;

    if (input.vertices == nullptr || input.vertexIndecis == nullptr || input.texCoords == nullptr || input.texCoordIndices == nullptr)
        return fail_with_message(error_message, "Full-color model input contains null geometry pointers.");
    if (input.materialDatas == nullptr)
        return fail_with_message(error_message, "Full-color model input contains no materials.");
    if (input.vertexCnt <= 0 || input.texCoordCnt <= 0 || input.triangleCnt <= 0 || input.materialCnt <= 0)
        return fail_with_message(error_message, "Full-color model input contains no usable geometry or materials.");
    if (input.vertexStride < static_cast<int32_t>(sizeof(float) * 3)
        || input.texCoordStride < static_cast<int32_t>(sizeof(float) * 2)
        || input.vertexIndexStride < static_cast<int32_t>(sizeof(int32_t))
        || input.texCoordIndexStride < static_cast<int32_t>(sizeof(int32_t))
        || input.faceIndexStride <= 0) {
        return fail_with_message(error_message, "Full-color model input contains invalid strides.");
    }

    source.vertices.resize(static_cast<size_t>(input.vertexCnt));
    for (int32_t vertex_idx = 0; vertex_idx < input.vertexCnt; ++vertex_idx) {
        const uint8_t *vertex = offset_pointer(input.vertices, static_cast<size_t>(vertex_idx) * input.vertexStride);
        for (size_t axis = 0; axis < 3; ++axis)
            source.vertices[vertex_idx][axis] = read_value<float>(vertex + axis * sizeof(float));
    }

    source.texture_coords.resize(static_cast<size_t>(input.texCoordCnt));
    for (int32_t tex_coord_idx = 0; tex_coord_idx < input.texCoordCnt; ++tex_coord_idx) {
        const uint8_t *tex_coord = offset_pointer(input.texCoords, static_cast<size_t>(tex_coord_idx) * input.texCoordStride);
        for (size_t axis = 0; axis < 2; ++axis)
            source.texture_coords[tex_coord_idx][axis] = read_value<float>(tex_coord + axis * sizeof(float));
    }

    source.triangles.resize(static_cast<size_t>(input.triangleCnt));
    source.tex_coord_indices.resize(static_cast<size_t>(input.triangleCnt));
    for (int32_t face_idx = 0; face_idx < input.triangleCnt; ++face_idx) {
        const uint8_t *vertex_indices = offset_pointer(input.vertexIndecis, static_cast<size_t>(face_idx) * input.faceIndexStride);
        const uint8_t *tex_coord_indices = offset_pointer(input.texCoordIndices, static_cast<size_t>(face_idx) * input.faceIndexStride);
        for (size_t corner = 0; corner < 3; ++corner) {
            const int32_t vertex_idx = read_value<int32_t>(vertex_indices + corner * input.vertexIndexStride);
            const int32_t tex_coord_idx = read_value<int32_t>(tex_coord_indices + corner * input.texCoordIndexStride);
            if (vertex_idx < 0 || vertex_idx >= input.vertexCnt)
                return fail_with_message(error_message, "Full-color model contains an invalid vertex index.");
            if (tex_coord_idx < 0 || tex_coord_idx >= input.texCoordCnt)
                return fail_with_message(error_message, "Full-color model contains an invalid texture-coordinate index.");
            source.triangles[face_idx][corner] = vertex_idx;
            source.tex_coord_indices[face_idx][corner] = tex_coord_idx;
        }
    }

    std::unordered_map<const in_texture_data_t *, int32_t> texture_indices;
    source.materials.reserve(static_cast<size_t>(input.materialCnt));
    source.face_material_ids.resize(static_cast<size_t>(input.triangleCnt), -1);

    int32_t material_begin_face = 0;
    for (int32_t material_idx = 0; material_idx < input.materialCnt; ++material_idx) {
        const in_material_data_t &input_material = input.materialDatas[material_idx];
        if (input_material.textureData == nullptr)
            return fail_with_message(error_message, "Full-color model material contains no texture or base color.");
        if (input_material.endTriangleIndex <= material_begin_face || input_material.endTriangleIndex > input.triangleCnt)
            return fail_with_message(error_message, "Full-color model material contains an invalid face range.");

        const in_texture_data_t &input_texture = *input_material.textureData;
        FullColorMaterial material;
        if (input_texture.isSolidColor) {
            material.has_base_color = true;
            material.base_color     = input_texture.solidColor;
        } else {
            auto texture_it = texture_indices.find(input_material.textureData);
            if (texture_it == texture_indices.end()) {
                if (input_texture.bits == nullptr || input_texture.width <= 0 || input_texture.height <= 0 || input_texture.channels < 3
                    || input_texture.bytePerLine < input_texture.width * input_texture.channels) {
                    return fail_with_message(error_message, "Full-color model contains an invalid texture.");
                }

                const size_t row_size  = static_cast<size_t>(input_texture.bytePerLine);
                const size_t row_count = static_cast<size_t>(input_texture.height);
                if (row_count > std::numeric_limits<size_t>::max() / row_size)
                    return fail_with_message(error_message, "Full-color model texture is too large.");

                FullColorTexture texture;
                texture.width          = input_texture.width;
                texture.height         = input_texture.height;
                texture.channels       = input_texture.channels;
                texture.bytes_per_line = input_texture.bytePerLine;
                texture.wrap_u         = input_texture.wraps[0];
                texture.wrap_v         = input_texture.wraps[1];
                texture.flip_y         = input_texture.flipY;
                texture.pixels.assign(input_texture.bits, input_texture.bits + row_size * row_count);

                const int32_t texture_idx = static_cast<int32_t>(source.textures.size());
                source.textures.emplace_back(std::move(texture));
                texture_it = texture_indices.emplace(input_material.textureData, texture_idx).first;
            }
            material.texture_index = texture_it->second;
        }

        source.materials.emplace_back(material);
        for (int32_t face_idx = material_begin_face; face_idx < input_material.endTriangleIndex; ++face_idx)
            source.face_material_ids[face_idx] = material_idx;
        material_begin_face = input_material.endTriangleIndex;
    }

    if (material_begin_face != input.triangleCnt)
        return fail_with_message(error_message, "Full-color model materials do not cover all faces.");
    if (!source.valid(error_message))
        return false;

    source.source_id = next_source_id.fetch_add(1, std::memory_order_relaxed);
    output = std::move(source);
    return true;
}

bool FullColorModelSource::valid(std::string *error_message) const
{
    if (vertices.empty() || triangles.empty())
        return fail_with_message(error_message, "Full-color model source contains no geometry.");
    if (texture_coords.empty() || tex_coord_indices.size() != triangles.size())
        return fail_with_message(error_message, "Full-color model source contains incomplete UV data.");
    if (materials.empty() || face_material_ids.size() != triangles.size())
        return fail_with_message(error_message, "Full-color model source contains incomplete material data.");

    for (const Triangle &triangle : triangles) {
        for (int32_t vertex_idx : triangle) {
            if (vertex_idx < 0 || static_cast<size_t>(vertex_idx) >= vertices.size())
                return fail_with_message(error_message, "Full-color model source contains an invalid vertex index.");
        }
    }
    for (const Triangle &tex_coord_triangle : tex_coord_indices) {
        for (int32_t tex_coord_idx : tex_coord_triangle) {
            if (tex_coord_idx < 0 || static_cast<size_t>(tex_coord_idx) >= texture_coords.size())
                return fail_with_message(error_message, "Full-color model source contains an invalid texture-coordinate index.");
        }
    }
    for (const FullColorTexture &texture : textures) {
        if (!texture.valid())
            return fail_with_message(error_message, "Full-color model source contains an invalid texture.");
    }
    for (const FullColorMaterial &material : materials) {
        const bool has_texture = material.texture_index >= 0 && static_cast<size_t>(material.texture_index) < textures.size();
        if (!has_texture && !material.has_base_color)
            return fail_with_message(error_message, "Full-color model source contains an invalid material.");
    }
    for (int32_t material_idx : face_material_ids) {
        if (material_idx < 0 || static_cast<size_t>(material_idx) >= materials.size())
            return fail_with_message(error_message, "Full-color model source contains an invalid face material.");
    }
    return true;
}

bool FullColorModelSource::empty() const
{
    return vertices.empty() || triangles.empty();
}

void FullColorModelSource::clear()
{
    *this = FullColorModelSource();
}

}} // namespace Slic3r::GUI
