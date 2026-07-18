#ifndef slic3r_GUI_MulticolorModelDialog_hpp_
#define slic3r_GUI_MulticolorModelDialog_hpp_

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <wx/dialog.h>
#include <wx/glcanvas.h>
#include <wx/panel.h>
#include <wx/stattext.h>

#include "slic3r/GUI/ConvertModel/ConvertModel.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"

class wxSlider;
class wxBoxSizer;
class wxCheckBox;
class ComboBox;
class SpinInput;

namespace Slic3r { namespace GUI {

struct ColorQuantizationConfig
{
    int default_count{4};
    int min_count{1};
    int max_count{4};
    int auto_count{4};
    std::vector<int> selectable_counts{1, 2, 3, 4};
};

enum class MulticolorFilamentMappingSource
{
    AutoMatched,
    NewGenerated,
    ManualSelected
};

struct MulticolorFilamentMapping
{
    std::string quantized_color;
    int         existing_filament_index{-1};
    int         target_filament_index{-1};
    int         allocated_new_filament_index{-1};
    std::string filament_color;
    std::string filament_preset_name;
    bool        matched_existing{false};
    bool        create_new{false};
    MulticolorFilamentMappingSource source{MulticolorFilamentMappingSource::NewGenerated};
};

struct MulticolorImportResult
{
    out_model_data_t original_model;
    out_model_data_t quantized_model;
    cvt_colors_t     quantized_source_colors;
    cvt_colors_t     selected_colors;
    std::vector<MulticolorFilamentMapping> filament_mappings;
    int              selected_color_count{0};
    bool             skipped{false};
    bool             fallback_to_geometry_only{false};
};

struct MulticolorModelPrecomputedData
{
    out_model_data_t original_model;
    out_model_data_t quantized_model;
    cvt_colors_t     quantized_source_colors;
    cvt_colors_t     selected_colors;
    int              selected_color_count{0};
    bool             has_original_model{false};
    bool             has_quantized_model{false};
};

class MulticolorModelPreviewCanvas : public wxGLCanvas
{
public:
    explicit MulticolorModelPreviewCanvas(wxWindow *parent);
    ~MulticolorModelPreviewCanvas() override;

    void set_model(const out_model_data_t &model);
    void reset_view();

private:
    void on_paint(wxPaintEvent &event);
    void on_size(wxSizeEvent &event);
    void on_left_down(wxMouseEvent &event);
    void on_left_up(wxMouseEvent &event);
    void on_mouse_move(wxMouseEvent &event);
    void on_mouse_wheel(wxMouseEvent &event);
    void on_capture_lost(wxMouseCaptureLostEvent &event);
    void render();
    void update_bounds();

private:
    wxGLContext *m_context{nullptr};
    out_model_data_t m_model;
    std::array<float, 3> m_center{0.0f, 0.0f, 0.0f};
    float m_extent{1.0f};
    float m_rot_x{-90.0f};
    float m_rot_y{0.0f};
    float m_zoom{1.0f};
    wxPoint m_last_mouse;
    bool m_dragging{false};
};

class MulticolorModelDialog : public DPIDialog
{
public:
    MulticolorModelDialog(wxWindow *parent, ConvertModel &converter, convert_model_data_t &model_data, int initial_color_count,
        int new_filament_start_index = 0);
    MulticolorModelDialog(wxWindow *parent, ConvertModel &converter, convert_model_data_t &model_data, int initial_color_count,
        int new_filament_start_index, const MulticolorModelPrecomputedData *precomputed_data);

    const MulticolorImportResult &import_result() const { return m_result; }
    const cvt_colors_t &selected_colors() const { return m_result.selected_colors; }
    int selected_color_count() const { return m_result.selected_color_count; }

private:
    enum class PreviewMode { Quantized, Original };

    void on_dpi_changed(const wxRect &suggested_rect) override;
    void build_ui();
    void init_model_data(int initial_color_count, const MulticolorModelPrecomputedData *precomputed_data);
    void refresh_tab_style();
    void refresh_quantization_controls();
    void focus_color_count_input(bool select_all);
    void switch_preview(PreviewMode mode);
    bool rebuild_quantized_preview(int color_count);
    bool rebuild_original_preview();
    int clamp_color_count(int color_count) const;
    void set_pending_color_count(int color_count, bool mark_changed);
    void apply_pending_color_count(bool force = false);
    void auto_quantize();
    void style_color_count_button(FFButton *button, bool selected);
    void rebuild_filament_mappings(bool reset_new_numbering, bool update_choice_items = true);
    void rebuild_filament_mapping_rows();
    void update_filament_mapping_row(size_t row_index, bool update_choice_items);
    void update_filament_mapping_choices();
    void update_filament_mapping_selections();
    void select_filament_mapping(size_t row_index, int selection);
    bool rebuild_quantized_preview_from_mapping();
    void update_selected_colors_from_filament_mappings();
    void finalize_skip_result();
    void finalize_result(bool accepted);

private:
    struct FilamentMappingRow
    {
        wxPanel *row_panel{nullptr};
        wxPanel *source_swatch{nullptr};
        wxStaticText *source_label{nullptr};
        ComboBox *choice{nullptr};
    };

    ConvertModel &m_converter;
    convert_model_data_t &m_model_data;
    MulticolorImportResult m_result;

    MulticolorModelPreviewCanvas *m_canvas{nullptr};
    wxStaticText *m_quantized_tab{nullptr};
    wxStaticText *m_original_tab{nullptr};
    wxSlider *m_color_count_slider{nullptr};
    SpinInput *m_color_count_input{nullptr};
    wxStaticText *m_quantization_changed_tip{nullptr};
    wxCheckBox *m_auto_match_filament_chk{nullptr};
    wxPanel *m_filament_mapping_panel{nullptr};
    wxBoxSizer *m_filament_mapping_rows_sizer{nullptr};
    FFButton *m_auto_btn{nullptr};
    FFButton *m_apply_btn{nullptr};
    FFButton *m_import_btn{nullptr};
    std::vector<FilamentMappingRow> m_filament_mapping_rows;
    std::vector<std::pair<int, FFButton *>> m_color_count_buttons;
    ColorQuantizationConfig m_quantization_config;
    cvt_colors_t m_quantized_source_colors;
    PreviewMode m_preview_mode{PreviewMode::Quantized};
    int m_applied_color_count{4};
    int m_pending_color_count{4};
    int m_styled_color_count{-1};
    int m_next_new_filament_index{0};
    int m_new_filament_start_index{0};
    int m_max_new_filament_count{4};
    bool m_auto_match_existing_filaments{true};
    bool m_quantization_dirty{false};
    bool m_mapping_dirty{false};
    bool m_updating_color_count_controls{false};
    bool m_quantization_tip_highlighted{false};
};

}} // namespace Slic3r::GUI

#endif
