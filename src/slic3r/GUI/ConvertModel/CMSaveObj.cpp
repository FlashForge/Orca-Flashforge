#include "CMSaveObj.hpp"
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

bool CMSaveObj::saveObj(const out_model_data_t &outData, const wxString &outOBjPath,
    const wxString &outMtlPath)
{
    wxFile file(outOBjPath, wxFile::write);
    if (!file.IsOpened()) {
        BOOST_LOG_TRIVIAL(error) << "open output obj file error, " << outOBjPath.ToUTF8().data();
        return false;
    }
    if (!writeMtllib(file, outMtlPath)) {
        return false;
    }
    if (!writeVertex(file, outData)) {
        return false;
    }
    if (!writeFace(file, outData)) {
        return false;
    }
    if (!flushWriteBuf(file)) {
        return false;
    }
    if (!saveMtl(outData, outMtlPath)) {
        return false;
    }
    return true;
}

bool CMSaveObj::writeMtllib(wxFile &file, const wxString &outMtlPath)
{
    boost::filesystem::path u8MtlPath(outMtlPath.ToUTF8().data());
    std::string fileName = u8MtlPath.filename().string();
    char buf[256];
    snprintf(buf, sizeof(buf), "mtllib %s\n", fileName.c_str());
    return write(file, buf);
}

bool CMSaveObj::writeVertex(wxFile &file, const out_model_data_t &outData)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < outData.vertices.size(); ++i) {
        const std::array<float, 3> &vertex = outData.vertices[i];
        oss.str("");
        oss << "v " << vertex[0] << ' ' << vertex[1] << ' ' << vertex[2] << '\n';
        if (!write(file, oss.str().c_str())) {
            return false;
        }
    }
    return true;
}

bool CMSaveObj::writeFace(wxFile &file, const out_model_data_t &outData)
{
    char buf[256];
    std::vector<std::vector<int>> multiTriangles(outData.colors.size());
    for (size_t i = 0; i < outData.triangles.size(); ++i) {
        multiTriangles[outData.triangles[i].colorIndex].push_back(i);
    }
    for (size_t i = 0; i < multiTriangles.size(); ++i) {
        snprintf(buf, sizeof(buf), "usemtl #%x%x%x\n",
            outData.colors[i][0], outData.colors[i][1], outData.colors[i][2]);
        if (!write(file, buf)) {
            return false;
        }
        const std::vector<int> &triangles = multiTriangles[i];
        for (size_t j = 0; j < triangles.size(); ++j) {
            const int32_t *vertexIndices = outData.triangles[triangles[j]].vertexIndices;
            snprintf(buf, sizeof(buf), "f %d %d %d\n",
                vertexIndices[0] + 1, vertexIndices[1] + 1, vertexIndices[2] + 1);
            if (!write(file, buf)) {
                return false;
            }
        }
    }
    return true;
}

bool CMSaveObj::saveMtl(const out_model_data_t &outData, const wxString &outMtlPath)
{
    wxFile file(outMtlPath, wxFile::write);
    if (!file.IsOpened()) {
        BOOST_LOG_TRIVIAL(error) << "open output mtl file error, " << outMtlPath.ToUTF8().data();
        return false;
    }
    char buf[256];
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < outData.colors.size(); ++i) {
        const std::array<uint8_t, 3> &color = outData.colors[i];
        snprintf(buf, sizeof(buf), "newmtl #%x%x%x\n", color[0], color[1], color[2]);
        if (!write(file, buf)) {
            return false;
        }
        oss.str("");
        oss << "Kd " << color[0] / 255.0 << ' ' << color[1] / 255.0 << ' ' << color[2] / 255.0 << '\n';
        if (!write(file, oss.str().c_str())) {
            return false;
        }
    }
    return flushWriteBuf(file);
}

bool CMSaveObj::write(wxFile &file,const char *buf)
{
    m_tmpWriteBuf += buf;
    if (m_tmpWriteBuf.size() < 102400) {
        return true;
    }
    if (!file.Write(m_tmpWriteBuf.data(), m_tmpWriteBuf.size())) {
        BOOST_LOG_TRIVIAL(error) << "write file error";
        return false;
    }
    m_tmpWriteBuf.clear();
    return true;
}

bool CMSaveObj::flushWriteBuf(wxFile &file)
{
    if (m_tmpWriteBuf.empty()) {
        return true;
    }
    if (!file.Write(m_tmpWriteBuf.data(), m_tmpWriteBuf.size())) {
        BOOST_LOG_TRIVIAL(error) << "write file error";
        return false;
    }
    m_tmpWriteBuf.clear();
    return true;
}

}} // namespace Slic3r::GUI
