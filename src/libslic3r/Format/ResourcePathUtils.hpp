#ifndef slic3r_Format_ResourcePathUtils_hpp_
#define slic3r_Format_ResourcePathUtils_hpp_

#include <algorithm>
#include <cctype>
#include <string>

#include <boost/filesystem.hpp>

namespace Slic3r { namespace resource_path {

inline int hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

inline std::string decode_resource_path(const std::string &raw_path)
{
    std::string value = raw_path;
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lower.rfind("file://", 0) == 0) {
        value.erase(0, 7);
        if (value.size() >= 3 && value[0] == '/' && std::isalpha(static_cast<unsigned char>(value[1])) && value[2] == ':')
            value.erase(value.begin());
    }

    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const int hi = hex_digit_value(value[i + 1]);
            const int lo = hex_digit_value(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        decoded.push_back(value[i] == '\\' ? '/' : value[i]);
    }
    return decoded;
}

inline boost::filesystem::path resolve_external_resource_path(const boost::filesystem::path &base_dir, const std::string &raw_path)
{
    const boost::filesystem::path requested(decode_resource_path(raw_path));
    boost::filesystem::path candidate = requested.is_absolute() ? requested : base_dir / requested;
    if (boost::filesystem::exists(candidate))
        return candidate;
    candidate = base_dir / requested.filename();
    return boost::filesystem::exists(candidate) ? candidate : boost::filesystem::path();
}

}} // namespace Slic3r::resource_path

#endif // slic3r_Format_ResourcePathUtils_hpp_
