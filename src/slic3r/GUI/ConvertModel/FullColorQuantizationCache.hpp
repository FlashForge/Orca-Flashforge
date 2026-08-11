#ifndef slic3r_GUI_FullColorQuantizationCache_hpp_
#define slic3r_GUI_FullColorQuantizationCache_hpp_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "FullColorQuantization.hpp"

namespace Slic3r { namespace GUI {

struct QuantizationCacheKey
{
    uint64_t                     source_id{ 0 };
    FullColorQuantizationBackend backend{ FullColorQuantizationBackend::Classic };
    int32_t                      color_count{ 0 };
    uint32_t                     smooth_weight_bits{ 0 };
    size_t                       min_oversampled_face_count{ 0 };
    size_t                       max_oversampled_face_count{ 0 };
    bool                         repair_mesh{ false };

    bool     trans_coord_sys{ false };
    bool     scale_model_size{ false };
    uint32_t min_model_extent_bits{ 0 };
    uint32_t max_print_size_bits[3]{ 0, 0, 0 };
    uint32_t subdivide_max_edge_len_ratio_bits{ 0 };
    uint32_t color_min_area_ratio_bits{ 0 };

    static QuantizationCacheKey from(const FullColorModelSource &source,
        FullColorQuantizationBackend backend, const QuantizationSettings &settings);

    bool operator==(const QuantizationCacheKey &other) const noexcept;
};

struct QuantizationCacheKeyHash
{
    size_t operator()(const QuantizationCacheKey &key) const noexcept;
};

class FullColorQuantizationCache
{
public:
    explicit FullColorQuantizationCache(size_t max_entries = 16);

    std::shared_ptr<const QuantizationResult> find(const QuantizationCacheKey &key);
    void insert(const QuantizationCacheKey &key, std::shared_ptr<const QuantizationResult> result);
    void clear();

    size_t size() const noexcept { return m_entries.size(); }

private:
    struct Entry
    {
        std::shared_ptr<const QuantizationResult> result;
        uint64_t                                  last_used{ 0 };
    };

    void evict_if_needed();

private:
    size_t m_max_entries{ 16 };
    uint64_t m_access_counter{ 0 };
    std::unordered_map<QuantizationCacheKey, Entry, QuantizationCacheKeyHash> m_entries;
};

}} // namespace Slic3r::GUI

#endif
