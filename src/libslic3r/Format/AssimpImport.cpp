#include "AssimpImport.hpp"

#include "../BambuTexturePainting.hpp"
#include "ResourcePathUtils.hpp"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <utility>

namespace Slic3r { namespace {

void clear_textured_mesh(TexturedMesh &out)
{
    out.vertices.clear();
    out.indices.clear();
    out.uvs.clear();
    out.uv_coords.clear();
    out.uv_indices.clear();
    out.textures.clear();
    out.material_ids.clear();
    out.material_texture_map.clear();
    out.material_colors.clear();
}

unsigned int assimp_import_flags(const std::string &path)
{
    unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_PreTransformVertices | aiProcess_SortByPType;
    if (boost::algorithm::iends_with(path, ".fbx"))
        flags |= aiProcess_FlipUVs;
    return flags;
}

void configure_importer(Assimp::Importer &importer, const std::string &path)
{
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    importer.SetPropertyBool(AI_CONFIG_PP_PTV_KEEP_HIERARCHY, true);
    if (boost::algorithm::iends_with(path, ".fbx")) {
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, true);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_MATERIALS, true);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_TEXTURES, true);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_ANIMATIONS, false);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_LIGHTS, false);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_CAMERAS, false);
    }
}

bool read_external_texture(const boost::filesystem::path &path, TextureImage &out)
{
    boost::nowide::ifstream file(path.string(), std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;
    const std::streamoff size = file.tellg();
    if (size <= 0 || static_cast<uintmax_t>(size) > static_cast<uintmax_t>(std::numeric_limits<size_t>::max()))
        return false;
    file.seekg(0);
    out.width = -1;
    out.height = -1;
    out.channels = 0;
    out.data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(out.data.data()), size);
    return static_cast<bool>(file) || file.eof();
}

bool read_embedded_texture(const aiTexture &texture, TextureImage &out)
{
    if (texture.mHeight == 0) {
        if (texture.mWidth == 0)
            return false;
        out.width = -1;
        out.height = -1;
        out.channels = 0;
        const auto *bytes = reinterpret_cast<const unsigned char *>(texture.pcData);
        out.data.assign(bytes, bytes + texture.mWidth);
        return true;
    }
    if (texture.mWidth == 0 || texture.mHeight == 0 ||
        texture.mWidth > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        texture.mHeight > static_cast<unsigned int>(std::numeric_limits<int>::max()))
        return false;
    const size_t width = texture.mWidth;
    const size_t height = texture.mHeight;
    if (width > std::numeric_limits<size_t>::max() / height || width * height > std::numeric_limits<size_t>::max() / 4)
        return false;
    out.width = static_cast<int>(texture.mWidth);
    out.height = static_cast<int>(texture.mHeight);
    out.channels = 4;
    out.data.resize(width * height * 4);
    for (size_t i = 0; i < width * height; ++i) {
        const aiTexel &texel = texture.pcData[i];
        out.data[i * 4 + 0] = texel.r;
        out.data[i * 4 + 1] = texel.g;
        out.data[i * 4 + 2] = texel.b;
        out.data[i * 4 + 3] = texel.a;
    }
    return true;
}

bool get_material_texture(const aiMaterial &material, aiString &texture_path)
{
    if (material.GetTextureCount(aiTextureType_DIFFUSE) > 0 && material.GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == AI_SUCCESS)
        return true;
    return material.GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
           material.GetTexture(aiTextureType_BASE_COLOR, 0, &texture_path) == AI_SUCCESS;
}

std::array<float, 4> get_material_color(const aiMaterial &material)
{
    aiColor4D color(1.f, 1.f, 1.f, 1.f);
    if (material.Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS || material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
        return {color.r, color.g, color.b, color.a};
    return {1.f, 1.f, 1.f, 1.f};
}

bool collect_mesh(const aiMesh &mesh, size_t &vertex_offset, TexturedMesh &out, std::string &error)
{
    if (vertex_offset > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        mesh.mNumVertices > static_cast<size_t>(std::numeric_limits<int>::max()) - vertex_offset) {
        error = "Assimp mesh has too many vertices";
        return false;
    }
    for (unsigned int i = 0; i < mesh.mNumVertices; ++i) {
        const aiVector3D &vertex = mesh.mVertices[i];
        out.vertices.push_back({vertex.x, vertex.y, vertex.z});
        if (mesh.HasTextureCoords(0)) {
            const aiVector3D &uv = mesh.mTextureCoords[0][i];
            out.uvs.push_back({uv.x, uv.y});
        } else {
            out.uvs.push_back({0.f, 0.f});
        }
    }
    for (unsigned int i = 0; i < mesh.mNumFaces; ++i) {
        const aiFace &face = mesh.mFaces[i];
        if (face.mNumIndices != 3)
            continue;
        if (face.mIndices[0] >= mesh.mNumVertices || face.mIndices[1] >= mesh.mNumVertices || face.mIndices[2] >= mesh.mNumVertices) {
            error = "Assimp mesh face index is out of bounds";
            return false;
        }
        out.indices.push_back({static_cast<int>(face.mIndices[0] + vertex_offset), static_cast<int>(face.mIndices[1] + vertex_offset),
                               static_cast<int>(face.mIndices[2] + vertex_offset)});
        out.material_ids.push_back(static_cast<int>(mesh.mMaterialIndex));
    }
    vertex_offset += mesh.mNumVertices;
    return true;
}

void collect_materials(const aiScene &scene, const boost::filesystem::path &base_dir, TexturedMesh &out)
{
    out.material_texture_map.assign(scene.mNumMaterials, -1);
    out.material_colors.assign(scene.mNumMaterials, {1.f, 1.f, 1.f, 1.f});
    for (unsigned int index = 0; index < scene.mNumMaterials; ++index) {
        const aiMaterial *material = scene.mMaterials[index];
        if (!material)
            continue;
        out.material_colors[index] = get_material_color(*material);
        aiString texture_path;
        if (!get_material_texture(*material, texture_path))
            continue;
        TextureImage image;
        if (const aiTexture *embedded = scene.GetEmbeddedTexture(texture_path.C_Str())) {
            if (!read_embedded_texture(*embedded, image))
                continue;
        } else {
            const boost::filesystem::path resolved = resource_path::resolve_external_resource_path(base_dir, texture_path.C_Str());
            if (resolved.empty() || !read_external_texture(resolved, image)) {
                BOOST_LOG_TRIVIAL(warning) << "AssimpImport: failed to read texture " << texture_path.C_Str();
                continue;
            }
        }
        out.material_texture_map[index] = static_cast<int>(out.textures.size());
        out.textures.push_back(std::move(image));
    }
}

} // namespace

bool load_assimp_textured_model(const std::string &path, TexturedMesh &out, std::string *error_message)
{
    clear_textured_mesh(out);
    Assimp::Importer importer;
    configure_importer(importer, path);
    const aiScene *scene = importer.ReadFile(path, assimp_import_flags(path));
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode || scene->mNumMeshes == 0) {
        std::ostringstream message;
        message << "Assimp failed to import " << path;
        if (importer.GetErrorString()[0] != '\0')
            message << ": " << importer.GetErrorString();
        if (error_message)
            *error_message = message.str();
        BOOST_LOG_TRIVIAL(error) << "AssimpImport: " << message.str();
        return false;
    }

    size_t vertex_offset = 0;
    for (unsigned int index = 0; index < scene->mNumMeshes; ++index) {
        const aiMesh *mesh = scene->mMeshes[index];
        if (!mesh || !mesh->HasPositions())
            continue;
        std::string error;
        if (!collect_mesh(*mesh, vertex_offset, out, error)) {
            if (error_message)
                *error_message = error;
            clear_textured_mesh(out);
            return false;
        }
    }
    if (out.vertices.empty() || out.indices.empty()) {
        if (error_message)
            *error_message = "Assimp extracted no valid triangles";
        clear_textured_mesh(out);
        return false;
    }
    collect_materials(*scene, boost::filesystem::path(path).parent_path(), out);
    BOOST_LOG_TRIVIAL(info) << "AssimpImport: loaded " << out.vertices.size() << " vertices, " << out.indices.size() << " triangles and "
                            << out.textures.size() << " textures from " << path;
    return true;
}

} // namespace Slic3r
