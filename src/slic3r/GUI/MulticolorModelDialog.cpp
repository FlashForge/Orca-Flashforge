#include <GL/glew.h>

#include "MulticolorModelDialog.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <boost/thread.hpp>
#include <wx/dcmemory.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dcclient.h>
#include <wx/slider.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>

#include "GUI_App.hpp"
#include "GUI_Utils.hpp"
#include "I18N.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "slic3r/Utils/ColorSpaceConvert.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/ProgressDialog.hpp"
#include "slic3r/GUI/Widgets/SpinInput.hpp"

namespace Slic3r { namespace GUI {
namespace {

constexpr const char *DefaultFilamentPresetName = "Flashforge PLA Basic";
constexpr float AutoMatchDeltaEThreshold = 5.0f;
// Keep imported full-color OBJ/GLB filament ids inside the 16 states supported by mmu segmentation.
constexpr int ImportExistingMatchLimit = 12;
constexpr int ImportFilamentLimit = 16;
constexpr intptr_t ExistingFilamentChoiceBase = 1;
constexpr intptr_t NewFilamentChoiceBase = 10000;

struct ExistingFilamentInfo
{
    int         index{-1};
    std::string color;
    std::string preset_name;
    std::string display_name;
};

struct ColorDistValue
{
    int   id{-1};
    float distance{0.0f};
};

static bool run_multicolor_apply_task(ProgressDialog &progress_dlg, const wxString &message, const std::function<bool()> &task)
{
    struct TaskState
    {
        std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> finished{false};
        bool success{false};
        std::exception_ptr exception;
    };

    auto task_state = std::make_shared<TaskState>();
    progress_dlg.Pulse(message);

    boost::thread worker_thread([task_state, &task]() {
        try {
            task_state->success = task();
        } catch (...) {
            task_state->exception = std::current_exception();
            task_state->success = false;
        }
        task_state->finished = true;
        task_state->condition.notify_all();
    });

    while (!task_state->finished) {
        {
            std::unique_lock<std::mutex> lock(task_state->mutex);
            task_state->condition.wait_for(lock, std::chrono::milliseconds(120));
        }
        progress_dlg.Pulse(message);
    }

    worker_thread.join();

    if (task_state->exception)
        std::rethrow_exception(task_state->exception);
    return task_state->success;
}

float clamp_zoom(float zoom)
{
    return std::clamp(zoom, 0.35f, 4.0f);
}

float normalize_degrees(float angle)
{
    angle = std::fmod(angle, 360.0f);
    return angle < 0.0f ? angle + 360.0f : angle;
}

std::array<float, 3> normal_of(const std::array<float, 3> &a, const std::array<float, 3> &b, const std::array<float, 3> &c)
{
    const std::array<float, 3> u{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const std::array<float, 3> v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    std::array<float, 3> n{
        u[1] * v[2] - u[2] * v[1],
        u[2] * v[0] - u[0] * v[2],
        u[0] * v[1] - u[1] * v[0]
    };
    const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len > 0.0f) {
        n[0] /= len;
        n[1] /= len;
        n[2] /= len;
    }
    return n;
}

wxFont tab_font(wxWindow *window, bool selected)
{
    wxFont font = window->GetFont();
    font.SetPointSize(font.GetPointSize() + 2);
    font.SetWeight(selected ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    return font;
}

int *preview_canvas_attributes()
{
    static int attributes[] = {
        WX_GL_RGBA,
        WX_GL_DOUBLEBUFFER,
        WX_GL_DEPTH_SIZE, 24,
        0
    };
    return attributes;
}

std::string color_to_hex(const cvt_color_t &color)
{
    return wxString::Format("#%02X%02X%02X", color[0], color[1], color[2]).ToStdString();
}

std::string normalize_hex_color(const std::string &color)
{
    wxColour wx_color(wxString::FromUTF8(color.c_str()));
    if (!wx_color.IsOk())
        return "#000000";
    return wx_color.GetAsString(wxC2S_HTML_SYNTAX).Upper().ToStdString();
}

wxColour wx_color_from_hex(const std::string &color)
{
    wxColour wx_color(wxString::FromUTF8(normalize_hex_color(color).c_str()));
    return wx_color.IsOk() ? wx_color : *wxBLACK;
}

wxBitmap filament_choice_bitmap(wxWindow *window, const std::string &color)
{
    const int size = window != nullptr ? window->FromDIP(28) : 28;
    wxBitmap bitmap(size, size);
    wxMemoryDC dc(bitmap);
    const wxColour wx_color = wx_color_from_hex(color);
    dc.SetBackground(wxBrush(wx_color));
    dc.Clear();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wx_color));
    dc.DrawRectangle(0, 0, size, size);
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

cvt_color_t cvt_color_from_hex(const std::string &color)
{
    const wxColour wx_color = wx_color_from_hex(color);
    return {
        static_cast<uint8_t>(wx_color.Red()),
        static_cast<uint8_t>(wx_color.Green()),
        static_cast<uint8_t>(wx_color.Blue())
    };
}

float calc_color_distance(const std::string &lhs, const std::string &rhs)
{
    const wxColour c1 = wx_color_from_hex(lhs);
    const wxColour c2 = wx_color_from_hex(rhs);
    float lab[2][3];
    RGB2Lab(c1.Red(), c1.Green(), c1.Blue(), &lab[0][0], &lab[0][1], &lab[0][2]);
    RGB2Lab(c2.Red(), c2.Green(), c2.Blue(), &lab[1][0], &lab[1][1], &lab[1][2]);
    return DeltaE76(lab[0][0], lab[0][1], lab[0][2], lab[1][0], lab[1][1], lab[1][2]);
}

std::string filament_display_name(const Preset *preset)
{
    if (preset == nullptr)
        return DefaultFilamentPresetName;
    if (!preset->alias.empty())
        return preset->alias;
    return preset->name;
}

std::vector<ExistingFilamentInfo> collect_existing_filaments()
{
    std::vector<ExistingFilamentInfo> filaments;
    PresetBundle *preset_bundle = wxGetApp().preset_bundle;
    if (preset_bundle == nullptr)
        return filaments;

    const ConfigOptionStrings *color_opt = preset_bundle->project_config.option<ConfigOptionStrings>("filament_colour");
    const size_t count = preset_bundle->filament_presets.size();
    filaments.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        ExistingFilamentInfo info;
        info.index = static_cast<int>(i);
        info.color = normalize_hex_color(color_opt != nullptr && i < color_opt->values.size() ? color_opt->values[i] : "#000000");
        info.preset_name = preset_bundle->filament_presets[i];
        info.display_name = filament_display_name(preset_bundle->filaments.find_preset(info.preset_name));
        filaments.push_back(info);
    }
    return filaments;
}

int find_best_existing_filament(const std::string &color, const std::vector<ExistingFilamentInfo> &filaments, const std::set<int> &used_existing)
{
    ColorDistValue best;
    best.distance = std::numeric_limits<float>::max();
    for (const ExistingFilamentInfo &filament : filaments) {
        if (used_existing.count(filament.index) > 0)
            continue;
        const float distance = calc_color_distance(color, filament.color);
        if (distance < best.distance) {
            best.id = filament.index;
            best.distance = distance;
        }
    }
    return best.id >= 0 && best.distance <= AutoMatchDeltaEThreshold ? best.id : -1;
}

const ExistingFilamentInfo *find_existing_filament(const std::vector<ExistingFilamentInfo> &filaments, int index)
{
    for (const ExistingFilamentInfo &filament : filaments)
        if (filament.index == index)
            return &filament;
    return nullptr;
}

std::vector<ExistingFilamentInfo> collect_matchable_existing_filaments(const std::vector<ExistingFilamentInfo> &filaments)
{
    std::vector<ExistingFilamentInfo> matchable;
    for (const ExistingFilamentInfo &filament : filaments)
        if (filament.index >= 0 && filament.index < ImportExistingMatchLimit)
            matchable.push_back(filament);
    return matchable;
}

bool is_matchable_existing_filament_index(int index)
{
    return index >= 0 && index < ImportExistingMatchLimit;
}

bool is_import_new_filament_index(int index)
{
    return index >= 0 && index < ImportFilamentLimit;
}

intptr_t existing_filament_choice_marker(int filament_index)
{
    return ExistingFilamentChoiceBase + filament_index;
}

intptr_t new_filament_choice_marker(int filament_index)
{
    return NewFilamentChoiceBase + filament_index;
}

bool is_existing_filament_choice(intptr_t marker)
{
    return marker >= ExistingFilamentChoiceBase && marker < NewFilamentChoiceBase;
}

bool is_new_filament_choice(intptr_t marker)
{
    return marker >= NewFilamentChoiceBase;
}

std::set<int> collect_allocated_new_filament_indices(const std::vector<MulticolorFilamentMapping> &mappings)
{
    std::set<int> indices;
    for (const MulticolorFilamentMapping &mapping : mappings)
        if (is_import_new_filament_index(mapping.allocated_new_filament_index))
            indices.insert(mapping.allocated_new_filament_index);
    return indices;
}

bool has_allocated_new_filament_index(const std::vector<MulticolorFilamentMapping> &mappings, int filament_index)
{
    for (const MulticolorFilamentMapping &mapping : mappings)
        if (mapping.allocated_new_filament_index == filament_index)
            return true;
    return false;
}

std::string allocated_new_filament_color_for_index(const std::vector<MulticolorFilamentMapping> &mappings, int filament_index)
{
    for (const MulticolorFilamentMapping &mapping : mappings) {
        if (mapping.allocated_new_filament_index == filament_index)
            return mapping.quantized_color.empty() ? std::string("#000000") : mapping.quantized_color;
    }
    return "#000000";
}

void populate_filament_mapping_choice(ComboBox *choice, const MulticolorFilamentMapping &mapping,
    const std::vector<ExistingFilamentInfo> &matchable_filaments, const std::set<int> &allocated_new_filament_indices,
    const std::vector<MulticolorFilamentMapping> &mappings)
{
    if (choice == nullptr)
        return;

    choice->Clear();
    int selected_choice = wxNOT_FOUND;

    const wxString current_group = _L("Current filament list");
    const wxString new_group = _L("New filament");
    if (!matchable_filaments.empty())
        choice->Append(current_group, wxNullBitmap, nullptr, DD_ITEM_STYLE_DISABLED);
    for (const ExistingFilamentInfo &filament : matchable_filaments) {
        const int item_idx = choice->Append(wxString::Format("%d  ", filament.index + 1) + wxString::FromUTF8(filament.display_name.c_str()),
            filament_choice_bitmap(choice, filament.color),
            reinterpret_cast<void *>(existing_filament_choice_marker(filament.index)));
        if (!mapping.create_new && mapping.target_filament_index == filament.index)
            selected_choice = item_idx;
    }

    if (!allocated_new_filament_indices.empty())
        choice->Append(new_group, wxNullBitmap, nullptr, DD_ITEM_STYLE_DISABLED);
    for (int filament_index : allocated_new_filament_indices) {
        const std::string new_filament_color = allocated_new_filament_color_for_index(mappings, filament_index);
        const int item_idx = choice->Append(wxString::Format("%d  ", filament_index + 1) + wxString::FromUTF8(DefaultFilamentPresetName),
            filament_choice_bitmap(choice, new_filament_color),
            reinterpret_cast<void *>(new_filament_choice_marker(filament_index)));
        if (mapping.create_new && mapping.target_filament_index == filament_index)
            selected_choice = item_idx;
    }

    choice->SetSelection(selected_choice);
}

} // namespace

MulticolorModelPreviewCanvas::MulticolorModelPreviewCanvas(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, preview_canvas_attributes(), wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    m_context = new wxGLContext(this);
    SetMinSize(wxSize(FromDIP(460), FromDIP(460)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    Bind(wxEVT_PAINT, &MulticolorModelPreviewCanvas::on_paint, this);
    Bind(wxEVT_SIZE, &MulticolorModelPreviewCanvas::on_size, this);
    Bind(wxEVT_LEFT_DOWN, &MulticolorModelPreviewCanvas::on_left_down, this);
    Bind(wxEVT_LEFT_UP, &MulticolorModelPreviewCanvas::on_left_up, this);
    Bind(wxEVT_MOTION, &MulticolorModelPreviewCanvas::on_mouse_move, this);
    Bind(wxEVT_MOUSEWHEEL, &MulticolorModelPreviewCanvas::on_mouse_wheel, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &MulticolorModelPreviewCanvas::on_capture_lost, this);
}

MulticolorModelPreviewCanvas::~MulticolorModelPreviewCanvas()
{
    delete m_context;
}

void MulticolorModelPreviewCanvas::set_model(const out_model_data_t &model)
{
    m_model = model;
    update_bounds();
    Refresh();
}

void MulticolorModelPreviewCanvas::reset_view()
{
    m_rot_x = -90.0f;
    m_rot_y = 0.0f;
    m_zoom = 1.0f;
    Refresh();
}

void MulticolorModelPreviewCanvas::on_paint(wxPaintEvent &)
{
    wxPaintDC dc(this);
    render();
}

void MulticolorModelPreviewCanvas::on_size(wxSizeEvent &event)
{
    Refresh();
    event.Skip();
}

void MulticolorModelPreviewCanvas::on_left_down(wxMouseEvent &event)
{
    m_dragging = true;
    m_last_mouse = event.GetPosition();
    if (!HasCapture())
        CaptureMouse();
}

void MulticolorModelPreviewCanvas::on_left_up(wxMouseEvent &event)
{
    m_dragging = false;
    if (HasCapture())
        ReleaseMouse();
    event.Skip();
}

void MulticolorModelPreviewCanvas::on_mouse_move(wxMouseEvent &event)
{
    if (!m_dragging || !event.Dragging() || !event.LeftIsDown()) {
        event.Skip();
        return;
    }

    const wxPoint pos = event.GetPosition();
    const wxPoint delta = pos - m_last_mouse;
    m_last_mouse = pos;
    m_rot_y = normalize_degrees(m_rot_y + static_cast<float>(delta.x) * 0.45f);
    m_rot_x += static_cast<float>(delta.y) * 0.45f;
    m_rot_x = std::clamp(m_rot_x, -90.0f, 90.0f);
    Refresh();
}

void MulticolorModelPreviewCanvas::on_mouse_wheel(wxMouseEvent &event)
{
    const int rotation = event.GetWheelRotation();
    m_zoom = clamp_zoom(m_zoom * (rotation > 0 ? 1.1f : 0.9f));
    Refresh();
}

void MulticolorModelPreviewCanvas::on_capture_lost(wxMouseCaptureLostEvent &event)
{
    m_dragging = false;
    event.Skip();
}

void MulticolorModelPreviewCanvas::update_bounds()
{
    if (m_model.vertices.empty()) {
        m_center = {0.0f, 0.0f, 0.0f};
        m_extent = 1.0f;
        return;
    }

    std::array<float, 3> min_pt{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    std::array<float, 3> max_pt{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };
    for (const auto &v : m_model.vertices) {
        for (int i = 0; i < 3; ++i) {
            min_pt[i] = std::min(min_pt[i], v[i]);
            max_pt[i] = std::max(max_pt[i], v[i]);
        }
    }

    for (int i = 0; i < 3; ++i)
        m_center[i] = 0.5f * (min_pt[i] + max_pt[i]);
    m_extent = std::max({max_pt[0] - min_pt[0], max_pt[1] - min_pt[1], max_pt[2] - min_pt[2], 1.0f});
}

void MulticolorModelPreviewCanvas::render()
{
    if (m_context == nullptr)
        return;
    SetCurrent(*m_context);

    int w = 1;
    int h = 1;
    GetClientSize(&w, &h);
#if defined(__APPLE__)
    const double scale = GetDPIScaleFactor();
    glViewport(0, 0, static_cast<GLsizei>(w * scale), static_cast<GLsizei>(h * scale));
#else
    glViewport(0, 0, w, h);
#endif

    glClearColor(0.93f, 0.93f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    const GLfloat light_pos[] = {0.2f, -0.4f, 1.0f, 0.0f};
    const GLfloat light_ambient[] = {0.42f, 0.42f, 0.42f, 1.0f};
    const GLfloat light_diffuse[] = {0.85f, 0.85f, 0.85f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = h > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    const float half = m_extent * 0.75f / m_zoom;
    if (aspect >= 1.0f)
        glOrtho(-half * aspect, half * aspect, -half, half, -m_extent * 6.0f, m_extent * 6.0f);
    else
        glOrtho(-half, half, -half / aspect, half / aspect, -m_extent * 6.0f, m_extent * 6.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(m_rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(m_rot_y, 0.0f, 1.0f, 0.0f);
    glTranslatef(-m_center[0], -m_center[1], -m_center[2]);

    glBegin(GL_TRIANGLES);
    for (const out_triangle_data_t &triangle : m_model.triangles) {
        if (triangle.colorIndex < 0 || triangle.colorIndex >= static_cast<int>(m_model.colors.size()))
            continue;

        bool valid = true;
        for (int vertex_idx : triangle.vertexIndices) {
            if (vertex_idx < 0 || vertex_idx >= static_cast<int>(m_model.vertices.size())) {
                valid = false;
                break;
            }
        }
        if (!valid)
            continue;

        const auto &color = m_model.colors[triangle.colorIndex];
        glColor3f(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f);

        const auto &a = m_model.vertices[triangle.vertexIndices[0]];
        const auto &b = m_model.vertices[triangle.vertexIndices[1]];
        const auto &c = m_model.vertices[triangle.vertexIndices[2]];
        const auto n = normal_of(a, b, c);
        glNormal3f(n[0], n[1], n[2]);
        glVertex3f(a[0], a[1], a[2]);
        glVertex3f(b[0], b[1], b[2]);
        glVertex3f(c[0], c[1], c[2]);
    }
    glEnd();

    glDisable(GL_LIGHTING);
    glFlush();
    SwapBuffers();
}

MulticolorModelDialog::MulticolorModelDialog(wxWindow *parent, ConvertModel &converter, convert_model_data_t &model_data,
    int initial_color_count, int new_filament_start_index)
    : DPIDialog(parent, wxID_ANY, _L("Import Model"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    , m_converter(converter)
    , m_model_data(model_data)
    , m_new_filament_start_index(std::clamp(new_filament_start_index, 0, ImportFilamentLimit))
    , m_max_new_filament_count(std::max(0, ImportFilamentLimit - m_new_filament_start_index))
{
    build_ui();
    init_model_data(initial_color_count, nullptr);
    Fit();
    CenterOnParent();
}

MulticolorModelDialog::MulticolorModelDialog(wxWindow *parent, ConvertModel &converter, convert_model_data_t &model_data,
    int initial_color_count, int new_filament_start_index, const MulticolorModelPrecomputedData *precomputed_data)
    : DPIDialog(parent, wxID_ANY, _L("Import Model"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    , m_converter(converter)
    , m_model_data(model_data)
    , m_new_filament_start_index(std::clamp(new_filament_start_index, 0, ImportFilamentLimit))
    , m_max_new_filament_count(std::max(0, ImportFilamentLimit - m_new_filament_start_index))
{
    build_ui();
    init_model_data(initial_color_count, precomputed_data);
    Fit();
    CenterOnParent();
}

void MulticolorModelDialog::build_ui()
{
    SetTitle(_L("Import Model"));
    SetFont(wxGetApp().normal_font());
    const std::string icon_path = resources_dir() + "/images/Orca-FlashforgeTitle.ico";
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));
    SetBackgroundColour(*wxWHITE);
    SetMinSize(wxSize(FromDIP(1160), FromDIP(640)));
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) {
        finalize_skip_result();
        EndModal(wxID_OK);
    });

    auto *root_sizer = new wxBoxSizer(wxVERTICAL);
    auto *content_sizer = new wxBoxSizer(wxHORIZONTAL);

    auto *preview_panel = new wxPanel(this);
    preview_panel->SetBackgroundColour(wxColour("#FAFAFA"));
    auto *preview_sizer = new wxBoxSizer(wxVERTICAL);
    auto *tabs_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_quantized_tab = new wxStaticText(preview_panel, wxID_ANY, _L("Color Quantization"));
    m_original_tab = new wxStaticText(preview_panel, wxID_ANY, _L("Original Model"));
    tabs_sizer->Add(m_quantized_tab, 0, wxRIGHT, FromDIP(28));
    tabs_sizer->Add(m_original_tab, 0, wxRIGHT, FromDIP(28));

    m_canvas = new MulticolorModelPreviewCanvas(preview_panel);
    preview_sizer->Add(tabs_sizer, 0, wxLEFT | wxTOP | wxBOTTOM, FromDIP(20));
    preview_sizer->Add(m_canvas, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    preview_panel->SetSizer(preview_sizer);

    m_quantized_tab->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) { switch_preview(PreviewMode::Quantized); });
    m_original_tab->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) { switch_preview(PreviewMode::Original); });

    auto *settings_panel = new wxPanel(this);
    settings_panel->SetBackgroundColour(*wxWHITE);
    settings_panel->SetMinSize(wxSize(FromDIP(520), -1));
    auto *settings_sizer = new wxBoxSizer(wxVERTICAL);

    auto *color_count_title = new wxStaticText(settings_panel, wxID_ANY, _L("Number of Colors"));
    wxFont color_count_title_font = color_count_title->GetFont();
    color_count_title_font.SetPointSize(color_count_title_font.GetPointSize() + 2);
    color_count_title_font.SetWeight(wxFONTWEIGHT_BOLD);
    color_count_title->SetFont(color_count_title_font);
    color_count_title->SetForegroundColour(wxColour("#333333"));
    settings_sizer->Add(color_count_title, 0, wxEXPAND | wxBOTTOM, FromDIP(14));

    const int max_selectable_color_count = std::max(m_quantization_config.min_count,
        std::min(m_quantization_config.max_count, m_max_new_filament_count));
    const int initial_pending_color_count = std::clamp(m_pending_color_count, m_quantization_config.min_count, max_selectable_color_count);

    auto *count_buttons_sizer = new wxBoxSizer(wxHORIZONTAL);
    for (int count : m_quantization_config.selectable_counts) {
        auto *count_btn = new FFButton(settings_panel, wxID_ANY, wxString::Format("%d", count), FromDIP(18), true);
        count_btn->SetSize(wxSize(FromDIP(56), FromDIP(36)));
        count_btn->SetMinSize(wxSize(FromDIP(56), FromDIP(36)));
        count_btn->SetMaxSize(wxSize(FromDIP(56), FromDIP(36)));
        count_btn->Enable(count <= max_selectable_color_count);
        count_btn->Bind(wxEVT_BUTTON, [this, count](wxCommandEvent &) {
            set_pending_color_count(count, true);
            focus_color_count_input(true);
        });
        m_color_count_buttons.emplace_back(count, count_btn);
        count_buttons_sizer->Add(count_btn, 0, wxRIGHT, FromDIP(10));
    }
    settings_sizer->Add(count_buttons_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(18));

    auto *count_input_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_color_count_slider = new wxSlider(settings_panel, wxID_ANY, initial_pending_color_count, m_quantization_config.min_count,
        max_selectable_color_count, wxDefaultPosition, wxSize(FromDIP(320), FromDIP(28)), wxSL_HORIZONTAL);
    m_color_count_slider->SetMinSize(wxSize(FromDIP(260), FromDIP(28)));
    m_color_count_slider->SetBackgroundColour(*wxWHITE);
    m_color_count_slider->Bind(wxEVT_SLIDER, [this](wxCommandEvent &) {
        if (!m_updating_color_count_controls)
            set_pending_color_count(m_color_count_slider->GetValue(), true);
    });
    m_color_count_input = new SpinInput(settings_panel, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(72), FromDIP(28)),
        wxTE_PROCESS_ENTER, m_quantization_config.min_count, max_selectable_color_count, initial_pending_color_count);
    m_color_count_input->SetRange(m_quantization_config.min_count, max_selectable_color_count);
    m_color_count_input->SetValue(initial_pending_color_count);
    m_color_count_input->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) {
        if (!m_updating_color_count_controls)
            set_pending_color_count(m_color_count_input->GetValue(), true);
    });
    wxTextCtrl *color_count_text_ctrl = m_color_count_input->GetTextCtrl();
    auto ensure_color_count_text_focus = [this](wxMouseEvent &event) {
        event.Skip();
        focus_color_count_input(false);
    };
    m_color_count_input->Bind(wxEVT_LEFT_DOWN, ensure_color_count_text_focus);
    color_count_text_ctrl->Bind(wxEVT_LEFT_DOWN, ensure_color_count_text_focus);
    color_count_text_ctrl->Bind(wxEVT_TEXT, [this, color_count_text_ctrl](wxCommandEvent &) {
        if (m_updating_color_count_controls)
            return;

        long value = 0;
        if (!color_count_text_ctrl->GetValue().ToLong(&value))
            return;
        if (value < m_color_count_input->GetMin() || value > m_color_count_input->GetMax())
            return;

        set_pending_color_count(static_cast<int>(value), true);
    });
    count_input_sizer->Add(m_color_count_slider, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    count_input_sizer->Add(m_color_count_input, 0, wxALIGN_CENTER_VERTICAL);
    settings_sizer->Add(count_input_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(14));

    m_quantization_changed_tip = new wxStaticText(settings_panel, wxID_ANY,
        _L("Parameters have changed, please click Apply to take effect"));
    m_quantization_changed_tip->Wrap(FromDIP(240));
    m_quantization_changed_tip->SetForegroundColour(*wxWHITE);
    m_quantization_changed_tip->SetMinSize(wxSize(-1, m_quantization_changed_tip->GetBestSize().GetHeight()));
    settings_sizer->Add(m_quantization_changed_tip, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    auto *quantization_button_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_auto_btn = new FFButton(settings_panel, wxID_ANY, _L("Auto"), FromDIP(15), true);
    m_apply_btn = new FFButton(settings_panel, wxID_ANY, _L("Apply"), FromDIP(15), true);
    for (FFButton *button : {m_auto_btn, m_apply_btn}) {
        button->SetFontUniformColor(wxColour("#15AEE5"));
        button->SetBorderUniformColor(wxColour("#15AEE5"));
        button->SetBGColor(*wxWHITE);
        button->SetBGHoverColor(wxColour("#EAF8FC"));
        button->SetBGPressColor(wxColour("#D5F1FA"));
        button->SetBGDisableColor(wxColour("#F5F5F5"));
        button->SetSize(wxSize(FromDIP(70), FromDIP(36)));
        button->SetMinSize(wxSize(FromDIP(70), FromDIP(36)));
        button->SetMaxSize(wxSize(FromDIP(70), FromDIP(36)));
    }
    m_auto_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { auto_quantize(); });
    m_apply_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { apply_pending_color_count(); });
    quantization_button_sizer->AddStretchSpacer();
    quantization_button_sizer->Add(m_auto_btn, 0, wxRIGHT, FromDIP(12));
    quantization_button_sizer->Add(m_apply_btn, 0);
    settings_sizer->Add(quantization_button_sizer, 0, wxEXPAND);

    auto *mapping_header_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto *mapping_title = new wxStaticText(settings_panel, wxID_ANY, _L("Filament Mapping"));
    wxFont mapping_title_font = mapping_title->GetFont();
    mapping_title_font.SetPointSize(mapping_title_font.GetPointSize() + 1);
    mapping_title_font.SetWeight(wxFONTWEIGHT_BOLD);
    mapping_title->SetFont(mapping_title_font);
    mapping_title->SetForegroundColour(wxColour("#333333"));
    m_auto_match_filament_chk = new wxCheckBox(settings_panel, wxID_ANY,
        _L("Auto match added filaments"));
    m_auto_match_filament_chk->SetValue(m_auto_match_existing_filaments);
    m_auto_match_filament_chk->SetBackgroundColour(*wxWHITE);
    m_auto_match_filament_chk->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &event) {
        m_auto_match_existing_filaments = event.IsChecked();
        rebuild_filament_mappings(false, false);
        m_mapping_dirty = true;
        refresh_quantization_controls();
    });
    mapping_header_sizer->Add(mapping_title, 0, wxALIGN_CENTER_VERTICAL);
    mapping_header_sizer->AddStretchSpacer();
    mapping_header_sizer->Add(m_auto_match_filament_chk, 0, wxALIGN_CENTER_VERTICAL);
    settings_sizer->Add(mapping_header_sizer, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(18));

    m_filament_mapping_panel = new wxPanel(settings_panel);
    m_filament_mapping_panel->SetBackgroundColour(*wxWHITE);
    m_filament_mapping_rows_sizer = new wxBoxSizer(wxVERTICAL);
    m_filament_mapping_panel->SetSizer(m_filament_mapping_rows_sizer);
    settings_sizer->Add(m_filament_mapping_panel, 0, wxEXPAND);
    settings_sizer->AddStretchSpacer();
    settings_panel->SetSizer(settings_sizer);

    content_sizer->Add(preview_panel, 1, wxEXPAND | wxRIGHT, FromDIP(24));
    content_sizer->Add(settings_panel, 0, wxEXPAND);

    auto *separator = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    separator->SetForegroundColour(wxColour("#DDDDDD"));
    separator->SetBackgroundColour(wxColour("#DDDDDD"));

    auto *button_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto *skip_btn = new FFButton(this, wxID_ANY, _L("Skip matching"), FromDIP(4), true);
    m_import_btn = new FFButton(this, wxID_ANY, _L("Confirm"), FromDIP(4), false);
    skip_btn->SetFontColor(wxColour("#333333"));
    skip_btn->SetFontHoverColor(wxColour("#333333"));
    skip_btn->SetFontPressColor(wxColour("#333333"));
    skip_btn->SetFontDisableColor(wxColour("#999999"));
    skip_btn->SetBorderUniformColor(wxColour("#DDDDDD"));
    skip_btn->SetBGColor(*wxWHITE);
    skip_btn->SetBGHoverColor(wxColour("#EEEEEE"));
    skip_btn->SetBGPressColor(wxColour("#CECECE"));
    skip_btn->SetBGDisableColor(wxColour("#F5F5F5"));
    skip_btn->SetSize(wxSize(FromDIP(136), FromDIP(44)));
    skip_btn->SetMinSize(wxSize(FromDIP(136), FromDIP(44)));
    skip_btn->SetMaxSize(wxSize(FromDIP(136), FromDIP(44)));
    m_import_btn->SetFontColor(wxColour("#ffffff"));
    m_import_btn->SetFontHoverColor(wxColour("#ffffff"));
    m_import_btn->SetFontPressColor(wxColour("#ffffff"));
    m_import_btn->SetFontDisableColor(wxColour("#ffffff"));
    m_import_btn->SetBGColor(wxColour("#419488"));
    m_import_btn->SetBGHoverColor(wxColour("#65A79E"));
    m_import_btn->SetBGPressColor(wxColour("#1A8676"));
    m_import_btn->SetBGDisableColor(wxColour("#dddddd"));
    m_import_btn->SetSize(wxSize(FromDIP(101), FromDIP(44)));
    m_import_btn->SetMinSize(wxSize(FromDIP(101), FromDIP(44)));
    m_import_btn->SetMaxSize(wxSize(FromDIP(101), FromDIP(44)));
    m_import_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        finalize_result(true);
        EndModal(wxID_OK);
    });
    skip_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        finalize_skip_result();
        EndModal(wxID_OK);
    });
    button_sizer->AddStretchSpacer();
    button_sizer->Add(skip_btn, 0, wxRIGHT, FromDIP(16));
    button_sizer->Add(m_import_btn, 0);

    root_sizer->AddSpacer(FromDIP(10));
    root_sizer->Add(content_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    root_sizer->AddSpacer(FromDIP(12));
    root_sizer->Add(separator, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    root_sizer->AddSpacer(FromDIP(45));
    root_sizer->Add(button_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
    root_sizer->AddSpacer(FromDIP(45));
    SetSizer(root_sizer);
    refresh_quantization_controls();
}

void MulticolorModelDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    SetSize(suggested_rect.GetSize());
    Layout();
    Fit();
}

void MulticolorModelDialog::init_model_data(int initial_color_count, const MulticolorModelPrecomputedData *precomputed_data)
{
    m_quantization_config.auto_count = m_quantization_config.default_count;
    m_applied_color_count = clamp_color_count(initial_color_count > 0 ? initial_color_count : m_quantization_config.default_count);
    m_pending_color_count = m_applied_color_count;

    bool has_quantized_preview = false;
    if (precomputed_data != nullptr) {
        if (precomputed_data->has_original_model)
            m_result.original_model = precomputed_data->original_model;
        else
            rebuild_original_preview();

        if (precomputed_data->has_quantized_model &&
            precomputed_data->selected_color_count == m_applied_color_count &&
            !precomputed_data->quantized_source_colors.empty()) {
            m_quantized_source_colors = precomputed_data->quantized_source_colors;
            m_result.quantized_source_colors = precomputed_data->quantized_source_colors;
            m_result.selected_colors = precomputed_data->selected_colors.empty() ?
                precomputed_data->quantized_source_colors : precomputed_data->selected_colors;
            m_result.selected_color_count = precomputed_data->selected_color_count;
            m_result.quantized_model = precomputed_data->quantized_model;
            has_quantized_preview = true;
        }
    } else {
        rebuild_original_preview();
    }

    if (!has_quantized_preview)
        rebuild_quantized_preview(m_applied_color_count);

    rebuild_filament_mappings(true);
    if (has_quantized_preview) {
        update_selected_colors_from_filament_mappings();
        if (m_result.selected_colors != m_quantized_source_colors)
            rebuild_quantized_preview_from_mapping();
    } else {
        rebuild_quantized_preview_from_mapping();
    }
    switch_preview(PreviewMode::Quantized);
    refresh_quantization_controls();
}

void MulticolorModelDialog::refresh_tab_style()
{
    const bool quantized = m_preview_mode == PreviewMode::Quantized;
    m_quantized_tab->SetForegroundColour(quantized ? wxColour(0, 174, 239) : wxColour(150, 150, 150));
    m_original_tab->SetForegroundColour(!quantized ? wxColour(0, 174, 239) : wxColour(150, 150, 150));
    m_quantized_tab->SetFont(tab_font(this, quantized));
    m_original_tab->SetFont(tab_font(this, !quantized));
}

void MulticolorModelDialog::refresh_quantization_controls()
{
    if (m_styled_color_count != m_pending_color_count) {
        for (auto &[count, button] : m_color_count_buttons) {
            if (m_styled_color_count < 0 || count == m_styled_color_count || count == m_pending_color_count)
                style_color_count_button(button, count == m_pending_color_count);
        }
        m_styled_color_count = m_pending_color_count;
    }

    if (m_color_count_slider != nullptr && m_color_count_slider->GetValue() != m_pending_color_count) {
        m_updating_color_count_controls = true;
        m_color_count_slider->SetValue(m_pending_color_count);
        m_updating_color_count_controls = false;
    }

    if (m_color_count_input != nullptr && m_color_count_input->GetValue() != m_pending_color_count) {
        m_updating_color_count_controls = true;
        m_color_count_input->SetValue(m_pending_color_count);
        m_updating_color_count_controls = false;
    }

    const bool controls_dirty = m_quantization_dirty || m_mapping_dirty;
    if (m_quantization_changed_tip != nullptr) {
        if (m_quantization_tip_highlighted != controls_dirty) {
            m_quantization_changed_tip->SetForegroundColour(controls_dirty ? wxColour("#F59A23") : *wxWHITE);
            m_quantization_changed_tip->Refresh();
            m_quantization_tip_highlighted = controls_dirty;
        }
    }
    if (m_apply_btn != nullptr) {
        if (m_apply_btn->IsEnabled() != controls_dirty)
            m_apply_btn->Enable(controls_dirty);
    }
    if (m_import_btn != nullptr) {
        if (m_import_btn->IsEnabled() == controls_dirty)
            m_import_btn->Enable(!controls_dirty);
    }
}

void MulticolorModelDialog::focus_color_count_input(bool select_all)
{
    if (m_color_count_input == nullptr)
        return;

    wxTextCtrl *text_ctrl = m_color_count_input->GetTextCtrl();
    text_ctrl->CallAfter([text_ctrl, select_all]() {
        if (!text_ctrl->IsShownOnScreen() || !text_ctrl->IsEnabled())
            return;

        text_ctrl->SetFocus();
        if (select_all)
            text_ctrl->SelectAll();
    });
}

void MulticolorModelDialog::switch_preview(PreviewMode mode)
{
    m_preview_mode = mode;
    refresh_tab_style();
    if (mode == PreviewMode::Quantized)
        m_canvas->set_model(m_result.quantized_model);
    else
        m_canvas->set_model(m_result.original_model);
}

bool MulticolorModelDialog::rebuild_original_preview()
{
    return m_converter.makePreviewModel(m_model_data, m_result.original_model);
}

bool MulticolorModelDialog::rebuild_quantized_preview(int color_count)
{
    const int clamped_color_count = clamp_color_count(color_count);
    m_quantized_source_colors = m_converter.clusterColors(m_model_data, clamped_color_count);
    m_result.quantized_source_colors = m_quantized_source_colors;
    m_result.selected_colors = m_quantized_source_colors;
    m_result.selected_color_count = clamped_color_count;
    if (m_result.selected_colors.empty())
        return false;
    return m_converter.makePreviewModel(m_model_data, m_result.quantized_model, m_result.selected_colors);
}

int MulticolorModelDialog::clamp_color_count(int color_count) const
{
    const int max_count = std::max(m_quantization_config.min_count,
        std::min(m_quantization_config.max_count, m_max_new_filament_count));
    return std::clamp(color_count, m_quantization_config.min_count, max_count);
}

void MulticolorModelDialog::set_pending_color_count(int color_count, bool mark_changed)
{
    m_pending_color_count = clamp_color_count(color_count);
    if (mark_changed)
        m_quantization_dirty = m_pending_color_count != m_applied_color_count;
    refresh_quantization_controls();
}

void MulticolorModelDialog::apply_pending_color_count(bool force)
{
    const bool color_count_changed = force || m_pending_color_count != m_applied_color_count || m_quantization_dirty;
    if (!color_count_changed && !m_mapping_dirty)
        return;

    const auto loading = _L("Loading") + dots;
    const wxString progress_message = _L("Generating preview...");
    ProgressDialog progress_dlg(loading, "", 100, find_toplevel_parent(this), wxPD_AUTO_HIDE | wxPD_APP_MODAL);
    auto rebuild_mapped_preview_with_progress = [&]() {
        update_selected_colors_from_filament_mappings();
        if (m_result.selected_colors.empty())
            return false;

        out_model_data_t mapped_model;
        const cvt_colors_t source_colors = m_quantized_source_colors;
        const cvt_colors_t selected_colors = m_result.selected_colors;
        const bool mapped_preview_ready = run_multicolor_apply_task(progress_dlg, progress_message, [&]() {
            return m_converter.makeMappedPreviewModel(m_model_data, mapped_model, source_colors, selected_colors);
        });
        if (!mapped_preview_ready)
            return false;

        m_result.quantized_model = std::move(mapped_model);
        return true;
    };

    if (color_count_changed) {
        const int clamped_color_count = clamp_color_count(m_pending_color_count);
        cvt_colors_t quantized_source_colors;
        out_model_data_t quantized_model;
        const bool quantized_preview_ready = run_multicolor_apply_task(progress_dlg, progress_message, [&]() {
            quantized_source_colors = m_converter.clusterColors(m_model_data, clamped_color_count);
            if (quantized_source_colors.empty())
                return false;
            return m_converter.makePreviewModel(m_model_data, quantized_model, quantized_source_colors);
        });
        if (!quantized_preview_ready)
            return;

        m_quantized_source_colors = std::move(quantized_source_colors);
        m_result.quantized_source_colors = m_quantized_source_colors;
        m_result.selected_colors = m_quantized_source_colors;
        m_result.selected_color_count = clamped_color_count;
        m_result.quantized_model = std::move(quantized_model);
        m_applied_color_count = m_pending_color_count;
        rebuild_filament_mappings(true);

        if (!rebuild_mapped_preview_with_progress())
            return;
    } else if (!rebuild_mapped_preview_with_progress())
        return;

    m_quantization_dirty = false;
    m_mapping_dirty = false;
    if (m_preview_mode == PreviewMode::Quantized)
        m_canvas->set_model(m_result.quantized_model);
    refresh_quantization_controls();
    focus_color_count_input(true);
}

void MulticolorModelDialog::auto_quantize()
{
    set_pending_color_count(m_quantization_config.auto_count, false);
    apply_pending_color_count(true);
}

void MulticolorModelDialog::style_color_count_button(FFButton *button, bool selected)
{
    if (button == nullptr)
        return;

    if (selected) {
        button->SetFontUniformColor(*wxWHITE);
        button->SetBorderUniformColor(wxColour("#15AEE5"));
        button->SetBGUniformColor(wxColour("#15AEE5"));
    } else {
        button->SetFontUniformColor(wxColour("#999999"));
        button->SetBorderUniformColor(wxColour("#BDBDBD"));
        button->SetBGUniformColor(*wxWHITE);
    }
}

void MulticolorModelDialog::rebuild_filament_mappings(bool reset_new_numbering, bool update_choice_items)
{
    const std::vector<ExistingFilamentInfo> existing_filaments = collect_existing_filaments();
    const std::vector<ExistingFilamentInfo> matchable_filaments = collect_matchable_existing_filaments(existing_filaments);
    const int new_filament_start = m_new_filament_start_index;
    if (reset_new_numbering) {
        m_next_new_filament_index = new_filament_start;
        m_result.filament_mappings.clear();
    }

    std::vector<MulticolorFilamentMapping> previous_mappings = m_result.filament_mappings;
    std::vector<MulticolorFilamentMapping> new_mappings;
    std::set<int> used_existing;
    std::set<int> used_allocated_new;

    auto reserve_allocated_new_filament = [this, &used_allocated_new](int filament_index) {
        if (!is_import_new_filament_index(filament_index))
            return false;
        if (!used_allocated_new.insert(filament_index).second)
            return false;
        m_next_new_filament_index = std::max(m_next_new_filament_index, filament_index + 1);
        return true;
    };

    auto next_allocated_new_filament = [this, new_filament_start, &used_allocated_new]() {
        if (m_next_new_filament_index < new_filament_start)
            m_next_new_filament_index = new_filament_start;
        for (int filament_index = m_next_new_filament_index; filament_index < ImportFilamentLimit; ++filament_index) {
            if (used_allocated_new.insert(filament_index).second) {
                m_next_new_filament_index = filament_index + 1;
                return filament_index;
            }
        }
        for (int filament_index = new_filament_start; filament_index < ImportFilamentLimit; ++filament_index) {
            if (used_allocated_new.insert(filament_index).second) {
                m_next_new_filament_index = filament_index + 1;
                return filament_index;
            }
        }
        return -1;
    };

    auto previous_new_filament_index = [](const MulticolorFilamentMapping *previous) {
        if (previous == nullptr)
            return -1;
        if (is_import_new_filament_index(previous->allocated_new_filament_index))
            return previous->allocated_new_filament_index;
        if (previous->create_new && is_import_new_filament_index(previous->target_filament_index))
            return previous->target_filament_index;
        return -1;
    };

    auto preserve_allocated_new_filament = [&](MulticolorFilamentMapping &mapping, const MulticolorFilamentMapping *previous) {
        if (reserve_allocated_new_filament(mapping.allocated_new_filament_index))
            return true;

        const int previous_filament_index = previous_new_filament_index(previous);
        if (reserve_allocated_new_filament(previous_filament_index)) {
            mapping.allocated_new_filament_index = previous_filament_index;
            return true;
        }

        if (is_import_new_filament_index(previous_filament_index)) {
            mapping.allocated_new_filament_index = next_allocated_new_filament();
            return is_import_new_filament_index(mapping.allocated_new_filament_index);
        }

        mapping.allocated_new_filament_index = -1;
        return false;
    };

    auto ensure_allocated_new_filament = [&](MulticolorFilamentMapping &mapping, const MulticolorFilamentMapping *previous) {
        if (preserve_allocated_new_filament(mapping, previous))
            return;
        mapping.allocated_new_filament_index = next_allocated_new_filament();
    };

    auto assign_new_filament = [&next_allocated_new_filament](MulticolorFilamentMapping &mapping) {
        if (!is_import_new_filament_index(mapping.allocated_new_filament_index))
            mapping.allocated_new_filament_index = next_allocated_new_filament();
        mapping.existing_filament_index = -1;
        mapping.matched_existing = false;
        mapping.create_new = true;
        mapping.filament_color = mapping.quantized_color;
        mapping.filament_preset_name = DefaultFilamentPresetName;
        mapping.source = MulticolorFilamentMappingSource::NewGenerated;
        mapping.target_filament_index = mapping.allocated_new_filament_index;
    };

    const cvt_colors_t &source_colors = !m_quantized_source_colors.empty() ? m_quantized_source_colors : m_result.selected_colors;
    const int current_new_filament_end = std::min(ImportFilamentLimit, new_filament_start + static_cast<int>(source_colors.size()));
    for (size_t source_color_index = 0; source_color_index < source_colors.size(); ++source_color_index) {
        const cvt_color_t &color = source_colors[source_color_index];
        MulticolorFilamentMapping mapping;
        mapping.quantized_color = normalize_hex_color(color_to_hex(color));
        mapping.allocated_new_filament_index = new_filament_start + static_cast<int>(source_color_index);
        if (!is_import_new_filament_index(mapping.allocated_new_filament_index))
            mapping.allocated_new_filament_index = -1;

        const MulticolorFilamentMapping *previous = nullptr;
        for (const MulticolorFilamentMapping &candidate : previous_mappings) {
            if (candidate.quantized_color == mapping.quantized_color) {
                previous = &candidate;
                break;
            }
        }

        if (previous != nullptr && previous->source == MulticolorFilamentMappingSource::ManualSelected && m_auto_match_existing_filaments) {
            mapping = *previous;
            const int allocated_new_filament_index = new_filament_start + static_cast<int>(source_color_index);
            mapping.allocated_new_filament_index = is_import_new_filament_index(allocated_new_filament_index) ?
                allocated_new_filament_index : -1;
            ensure_allocated_new_filament(mapping, previous);
            if (mapping.create_new) {
                if (mapping.target_filament_index < new_filament_start || mapping.target_filament_index >= current_new_filament_end)
                    assign_new_filament(mapping);
            } else if (const ExistingFilamentInfo *filament = find_existing_filament(matchable_filaments, mapping.target_filament_index)) {
                mapping.existing_filament_index = filament->index;
                mapping.filament_color = filament->color;
                mapping.filament_preset_name = filament->display_name;
                mapping.matched_existing = true;
                used_existing.insert(filament->index);
            } else {
                assign_new_filament(mapping);
                mapping.source = MulticolorFilamentMappingSource::ManualSelected;
            }
            new_mappings.push_back(mapping);
            continue;
        }

        int matched_index = -1;
        if (m_auto_match_existing_filaments)
            matched_index = find_best_existing_filament(mapping.quantized_color, matchable_filaments, used_existing);

        if (const ExistingFilamentInfo *filament = find_existing_filament(matchable_filaments, matched_index)) {
            mapping.existing_filament_index = filament->index;
            mapping.target_filament_index = filament->index;
            mapping.filament_color = filament->color;
            mapping.filament_preset_name = filament->display_name;
            mapping.matched_existing = true;
            mapping.create_new = false;
            mapping.source = MulticolorFilamentMappingSource::AutoMatched;
            used_existing.insert(filament->index);
        } else {
            assign_new_filament(mapping);
        }
        new_mappings.push_back(mapping);
    }

    m_result.filament_mappings = std::move(new_mappings);
    if (update_choice_items)
        update_filament_mapping_choices();
    else
        update_filament_mapping_selections();
}

void MulticolorModelDialog::update_selected_colors_from_filament_mappings()
{
    m_result.selected_colors.clear();
    m_result.selected_colors.reserve(m_result.filament_mappings.size());
    for (const MulticolorFilamentMapping &mapping : m_result.filament_mappings) {
        const std::string &color = mapping.filament_color.empty() ? mapping.quantized_color : mapping.filament_color;
        m_result.selected_colors.push_back(cvt_color_from_hex(color));
    }
    m_result.selected_color_count = static_cast<int>(m_result.selected_colors.size());
}

bool MulticolorModelDialog::rebuild_quantized_preview_from_mapping()
{
    update_selected_colors_from_filament_mappings();
    if (m_result.selected_colors.empty())
        return false;
    return m_converter.makeMappedPreviewModel(m_model_data, m_result.quantized_model,
        m_quantized_source_colors, m_result.selected_colors);
}

void MulticolorModelDialog::rebuild_filament_mapping_rows()
{
    if (m_filament_mapping_rows_sizer == nullptr)
        return;

    m_filament_mapping_rows_sizer->Clear(true);
    m_filament_mapping_rows.clear();
    m_filament_mapping_rows.reserve(m_result.filament_mappings.size());

    for (size_t row_index = 0; row_index < m_result.filament_mappings.size(); ++row_index) {
        const MulticolorFilamentMapping &mapping = m_result.filament_mappings[row_index];

        auto *row_panel = new wxPanel(m_filament_mapping_panel);
        row_panel->SetBackgroundColour(*wxWHITE);
        auto *row_sizer = new wxBoxSizer(wxHORIZONTAL);

        auto *source_panel = new wxPanel(row_panel, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(150), FromDIP(44)));
        source_panel->SetBackgroundColour(wxColour("#FAFAFA"));
        auto *source_sizer = new wxBoxSizer(wxHORIZONTAL);
        auto *source_swatch = new wxPanel(source_panel, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(28), FromDIP(28)));
        source_swatch->SetBackgroundColour(wx_color_from_hex(mapping.quantized_color));
        auto *source_label = new wxStaticText(source_panel, wxID_ANY, wxString::FromUTF8(mapping.quantized_color.c_str()));
        source_label->SetForegroundColour(wxColour("#555555"));
        source_sizer->Add(source_swatch, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(12));
        source_sizer->Add(source_label, 0, wxALIGN_CENTER_VERTICAL);
        source_panel->SetSizer(source_sizer);

        auto *arrow_label = new wxStaticText(row_panel, wxID_ANY, "->");
        arrow_label->SetForegroundColour(wxColour("#999999"));

        auto *target_panel = new wxPanel(row_panel, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(330), FromDIP(44)));
        target_panel->SetBackgroundColour(wxColour("#FAFAFA"));
        auto *target_sizer = new wxBoxSizer(wxHORIZONTAL);

        auto *choice = new ComboBox(target_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(330), FromDIP(44)), 0, nullptr, wxCB_READONLY);
        choice->SetBorderWidth(0);
        choice->SetCornerRadius(0);
        choice->SetBackgroundColorNormal(wxColour("#FAFAFA"));
        choice->UpdateTextCtrlColours();
        choice->GetDropDown().SetUseContentWidth(true);
        choice->Bind(wxEVT_COMBOBOX, [this, row_index, choice](wxCommandEvent &event) {
            const int selection = event.GetInt();
            if (selection < 0)
                return;
            void *client_data = choice->GetClientData(selection);
            if (client_data == nullptr)
                return;
            select_filament_mapping(row_index, static_cast<int>(reinterpret_cast<intptr_t>(client_data)));
        });
        target_sizer->Add(choice, 1, wxEXPAND);
        target_panel->SetSizer(target_sizer);

        row_sizer->Add(source_panel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        row_sizer->Add(arrow_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        row_sizer->Add(target_panel, 1, wxALIGN_CENTER_VERTICAL);
        row_panel->SetSizer(row_sizer);
        m_filament_mapping_rows_sizer->Add(row_panel, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
        m_filament_mapping_rows.push_back({row_panel, source_swatch, source_label, choice});
    }

    update_filament_mapping_choices();
    m_filament_mapping_panel->Layout();
    Layout();
    Fit();
}

void MulticolorModelDialog::update_filament_mapping_row(size_t row_index, bool update_choice_items)
{
    if (row_index >= m_result.filament_mappings.size() || row_index >= m_filament_mapping_rows.size())
        return;

    const MulticolorFilamentMapping &mapping = m_result.filament_mappings[row_index];
    FilamentMappingRow &row = m_filament_mapping_rows[row_index];
    if (row.source_swatch != nullptr) {
        row.source_swatch->SetBackgroundColour(wx_color_from_hex(mapping.quantized_color));
        row.source_swatch->Refresh();
    }
    if (row.source_label != nullptr)
        row.source_label->SetLabel(wxString::FromUTF8(mapping.quantized_color.c_str()));
    if (update_choice_items) {
        const std::vector<ExistingFilamentInfo> existing_filaments = collect_existing_filaments();
        const std::vector<ExistingFilamentInfo> matchable_filaments = collect_matchable_existing_filaments(existing_filaments);
        populate_filament_mapping_choice(row.choice, mapping, matchable_filaments,
            collect_allocated_new_filament_indices(m_result.filament_mappings), m_result.filament_mappings);
    }
    if (row.row_panel != nullptr)
        row.row_panel->Layout();
}

void MulticolorModelDialog::update_filament_mapping_choices()
{
    if (m_filament_mapping_rows.size() != m_result.filament_mappings.size()) {
        rebuild_filament_mapping_rows();
        return;
    }

    const std::vector<ExistingFilamentInfo> existing_filaments = collect_existing_filaments();
    const std::vector<ExistingFilamentInfo> matchable_filaments = collect_matchable_existing_filaments(existing_filaments);
    const std::set<int> allocated_new_filament_indices = collect_allocated_new_filament_indices(m_result.filament_mappings);
    for (size_t row_index = 0; row_index < m_result.filament_mappings.size(); ++row_index) {
        update_filament_mapping_row(row_index, false);
        populate_filament_mapping_choice(m_filament_mapping_rows[row_index].choice, m_result.filament_mappings[row_index],
            matchable_filaments, allocated_new_filament_indices, m_result.filament_mappings);
    }
}

void MulticolorModelDialog::update_filament_mapping_selections()
{
    if (m_filament_mapping_rows.size() != m_result.filament_mappings.size()) {
        rebuild_filament_mapping_rows();
        return;
    }

    for (size_t row_index = 0; row_index < m_result.filament_mappings.size(); ++row_index) {
        update_filament_mapping_row(row_index, false);

        ComboBox *choice = m_filament_mapping_rows[row_index].choice;
        if (choice == nullptr)
            continue;

        const MulticolorFilamentMapping &mapping = m_result.filament_mappings[row_index];
        const intptr_t target_marker = mapping.create_new ?
            new_filament_choice_marker(mapping.target_filament_index) :
            existing_filament_choice_marker(mapping.target_filament_index);
        int selected_choice = wxNOT_FOUND;
        for (unsigned int item_idx = 0; item_idx < choice->GetCount(); ++item_idx) {
            if (reinterpret_cast<intptr_t>(choice->GetClientData(item_idx)) == target_marker) {
                selected_choice = static_cast<int>(item_idx);
                break;
            }
        }
        if (choice->GetSelection() != selected_choice)
            choice->SetSelection(selected_choice);
    }
}

void MulticolorModelDialog::select_filament_mapping(size_t row_index, int selection)
{
    if (row_index >= m_result.filament_mappings.size())
        return;

    const std::vector<ExistingFilamentInfo> existing_filaments = collect_existing_filaments();
    const std::vector<ExistingFilamentInfo> matchable_filaments = collect_matchable_existing_filaments(existing_filaments);
    MulticolorFilamentMapping &mapping = m_result.filament_mappings[row_index];
    const intptr_t marker = static_cast<intptr_t>(selection);
    if (is_existing_filament_choice(marker)) {
        const int filament_index = static_cast<int>(marker - ExistingFilamentChoiceBase);
        if (!is_matchable_existing_filament_index(filament_index))
            return;
        const ExistingFilamentInfo *filament = find_existing_filament(matchable_filaments, filament_index);
        if (filament == nullptr)
            return;

        mapping.existing_filament_index = filament->index;
        mapping.target_filament_index = filament->index;
        mapping.filament_color = filament->color;
        mapping.filament_preset_name = filament->display_name;
        mapping.matched_existing = true;
        mapping.create_new = false;
        mapping.source = MulticolorFilamentMappingSource::ManualSelected;
    } else if (is_new_filament_choice(marker)) {
        const int filament_index = static_cast<int>(marker - NewFilamentChoiceBase);
        if (!is_import_new_filament_index(filament_index) ||
            !has_allocated_new_filament_index(m_result.filament_mappings, filament_index))
            return;

        mapping.existing_filament_index = -1;
        mapping.target_filament_index = filament_index;
        mapping.filament_color = allocated_new_filament_color_for_index(m_result.filament_mappings, filament_index);
        mapping.filament_preset_name = DefaultFilamentPresetName;
        mapping.matched_existing = false;
        mapping.create_new = true;
        mapping.source = MulticolorFilamentMappingSource::ManualSelected;
        m_next_new_filament_index = std::max(m_next_new_filament_index, mapping.allocated_new_filament_index + 1);
    } else {
        return;
    }

    m_mapping_dirty = true;
    refresh_quantization_controls();
    update_filament_mapping_row(row_index, true);
}

void MulticolorModelDialog::finalize_skip_result()
{
    const std::vector<ExistingFilamentInfo> existing_filaments = collect_existing_filaments();
    const ExistingFilamentInfo *first_filament = existing_filaments.empty() ? nullptr : &existing_filaments.front();
    const std::string filament_color = first_filament != nullptr ? first_filament->color : std::string("#000000");

    cvt_colors_t source_colors = m_converter.clusterColors(m_model_data, 1);
    if (source_colors.empty())
        source_colors.push_back(cvt_color_from_hex(filament_color));

    m_quantized_source_colors = source_colors;
    m_result.quantized_source_colors = source_colors;
    m_result.selected_colors = { cvt_color_from_hex(filament_color) };
    m_result.selected_color_count = 1;
    m_result.filament_mappings.clear();

    MulticolorFilamentMapping mapping;
    mapping.quantized_color = normalize_hex_color(color_to_hex(source_colors.front()));
    mapping.existing_filament_index = first_filament != nullptr ? first_filament->index : -1;
    mapping.target_filament_index = first_filament != nullptr ? first_filament->index : 0;
    mapping.filament_color = filament_color;
    mapping.filament_preset_name = first_filament != nullptr ? first_filament->display_name : DefaultFilamentPresetName;
    mapping.matched_existing = first_filament != nullptr;
    mapping.create_new = false;
    mapping.source = MulticolorFilamentMappingSource::AutoMatched;
    m_result.filament_mappings.push_back(mapping);

    m_result.skipped = true;
    m_result.fallback_to_geometry_only = false;
}

void MulticolorModelDialog::finalize_result(bool accepted)
{
    m_result.skipped = !accepted;
    m_result.fallback_to_geometry_only = false;
    m_result.selected_color_count = m_applied_color_count;
}

}} // namespace Slic3r::GUI
