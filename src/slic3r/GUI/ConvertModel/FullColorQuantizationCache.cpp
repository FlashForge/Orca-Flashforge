#include "FullColorQuantizationCache.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <utility>

namespace Slic3r { namespace GUI {

namespace {

uint32_t float_bits(float value)
{
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "Unexpected float size.");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void hash_combine(size_t &seed, size_t value)
{
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
}

} // namespace

QuantizationCacheKey QuantizationCacheKey::from(const FullColorModelSource &source,
    FullColorQuantizationBackend backend, const QuantizationSettings &settings)
{
    QuantizationCacheKey key;
    key.source_id = source.source_id;
    key.backend = backend;
    key.color_count = settings.color_count;
    key.smooth_weight_bits = float_bits(settings.smooth_weight);
    key.min_oversampled_face_count = settings.min_oversampled_face_count;
    key.max_oversampled_face_count = settings.max_oversampled_face_count;
    key.repair_mesh = settings.repair_mesh;

    const in_cvt_params_t &conversion = settings.conversion;
    key.trans_coord_sys = conversion.transCoordSys;
    key.scale_model_size = conversion.scaleModelSize;
    key.min_model_extent_bits = float_bits(conversion.minModelExtent);
    for (size_t axis = 0; axis < 3; ++axis)
        key.max_print_size_bits[axis] = float_bits(conversion.maxPrintSize[axis]);
    key.subdivide_max_edge_len_ratio_bits = float_bits(conversion.subdivideMaxEdgeLenRatio);
    key.color_min_area_ratio_bits = float_bits(conversion.colorMinAreaRatio);
    return key;
}

bool QuantizationCacheKey::operator==(const QuantizationCacheKey &other) const noexcept
{
    return source_id == other.source_id
        && backend == other.backend
        && color_count == other.color_count
        && smooth_weight_bits == other.smooth_weight_bits
        && min_oversampled_face_count == other.min_oversampled_face_count
        && max_oversampled_face_count == other.max_oversampled_face_count
        && repair_mesh == other.repair_mesh
        && trans_coord_sys == other.trans_coord_sys
        && scale_model_size == other.scale_model_size
        && min_model_extent_bits == other.min_model_extent_bits
        && max_print_size_bits[0] == other.max_print_size_bits[0]
        && max_print_size_bits[1] == other.max_print_size_bits[1]
        && max_print_size_bits[2] == other.max_print_size_bits[2]
        && subdivide_max_edge_len_ratio_bits == other.subdivide_max_edge_len_ratio_bits
        && color_min_area_ratio_bits == other.color_min_area_ratio_bits;
}

size_t QuantizationCacheKeyHash::operator()(const QuantizationCacheKey &key) const noexcept
{
    size_t seed = 0;
    hash_combine(seed, std::hash<uint64_t>{}(key.source_id));
    hash_combine(seed, std::hash<uint8_t>{}(static_cast<uint8_t>(key.backend)));
    hash_combine(seed, std::hash<int32_t>{}(key.color_count));
    hash_combine(seed, std::hash<uint32_t>{}(key.smooth_weight_bits));
    hash_combine(seed, std::hash<size_t>{}(key.min_oversampled_face_count));
    hash_combine(seed, std::hash<size_t>{}(key.max_oversampled_face_count));
    hash_combine(seed, std::hash<bool>{}(key.repair_mesh));
    hash_combine(seed, std::hash<bool>{}(key.trans_coord_sys));
    hash_combine(seed, std::hash<bool>{}(key.scale_model_size));
    hash_combine(seed, std::hash<uint32_t>{}(key.min_model_extent_bits));
    for (uint32_t value : key.max_print_size_bits)
        hash_combine(seed, std::hash<uint32_t>{}(value));
    hash_combine(seed, std::hash<uint32_t>{}(key.subdivide_max_edge_len_ratio_bits));
    hash_combine(seed, std::hash<uint32_t>{}(key.color_min_area_ratio_bits));
    return seed;
}

FullColorQuantizationCache::FullColorQuantizationCache(size_t max_entries)
    : m_max_entries(std::max<size_t>(1, max_entries))
{}

std::shared_ptr<const QuantizationResult> FullColorQuantizationCache::find(const QuantizationCacheKey &key)
{
    const auto found = m_entries.find(key);
    if (found == m_entries.end())
        return {};
    found->second.last_used = ++m_access_counter;
    return found->second.result;
}

void FullColorQuantizationCache::insert(const QuantizationCacheKey &key,
    std::shared_ptr<const QuantizationResult> result)
{
    if (result == nullptr || !result->valid() || result->source == nullptr)
        return;
    const QuantizationCacheKey result_key = QuantizationCacheKey::from(
        *result->source, result->backend, result->settings);
    if (!(result_key == key))
        return;

    Entry &entry = m_entries[key];
    entry.result = std::move(result);
    entry.last_used = ++m_access_counter;
    evict_if_needed();
}

void FullColorQuantizationCache::clear()
{
    m_entries.clear();
    m_access_counter = 0;
}

void FullColorQuantizationCache::evict_if_needed()
{
    while (m_entries.size() > m_max_entries) {
        auto oldest = std::min_element(m_entries.begin(), m_entries.end(),
            [](const auto &lhs, const auto &rhs) {
                return lhs.second.last_used < rhs.second.last_used;
            });
        if (oldest == m_entries.end())
            return;
        m_entries.erase(oldest);
    }
}

}} // namespace Slic3r::GUI
