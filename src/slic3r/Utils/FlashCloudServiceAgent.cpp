#include "FlashCloudServiceAgent.hpp"

#include "libslic3r/Preset.hpp"
#include "../GUI/FlashForge/MultiComMgr.hpp"
#include "../GUI/FlashForge/WanDevTokenMgr.hpp"

#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>

namespace Slic3r {
namespace {

constexpr int PRESET_SYNC_TIMEOUT_MS = 30000;

struct SyncCallbacks
{
    CheckFn* check;
    ProgressFn* progress;
    WasCancelledFn* cancel;
};

fnet::FlashNetworkIntfc* network_interface() { return GUI::MultiComMgr::inst()->networkIntfc(); }

int check_callback(const char* setting_json, void* data)
{
    auto* callbacks = static_cast<SyncCallbacks*>(data);
    if (!callbacks || !callbacks->check || !*callbacks->check || !setting_json)
        return 0;

    try {
        const nlohmann::json json = nlohmann::json::parse(setting_json);
        std::map<std::string, std::string> setting;
        for (const auto& [key, value] : json.items())
            setting[key] = value.is_string() ? value.get<std::string>() : value.dump();
        (*callbacks->check)(setting);
        return 0;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent: invalid setting callback JSON: " << e.what();
        return FNET_INVALID_PARAMETER;
    }
}

void progress_callback(int progress, void* data)
{
    auto* callbacks = static_cast<SyncCallbacks*>(data);
    if (callbacks && callbacks->progress && *callbacks->progress)
        (*callbacks->progress)(progress);
}

int cancel_callback(void* data)
{
    auto* callbacks = static_cast<SyncCallbacks*>(data);
    return callbacks && callbacks->cancel && *callbacks->cancel && (*callbacks->cancel)() ? 1 : 0;
}

nlohmann::json values_to_json(const std::map<std::string, std::string>* values)
{
    nlohmann::json json = nlohmann::json::object();
    if (values) {
        for (const auto& [key, value] : *values)
            json[key] = value;
    }
    return json;
}

void log_sync_json(const char* operation, const nlohmann::json& request)
{
    BOOST_LOG_TRIVIAL(info) << "FlashCloudServiceAgent::" << operation << " request=" << request.dump();
}

void log_sync_result(const char* operation, int result, unsigned int http_code, const char* response = nullptr)
{
    BOOST_LOG_TRIVIAL(info) << "FlashCloudServiceAgent::" << operation << " result=" << result << " http_code=" << http_code
                            << " response=" << (response ? response : "");
}
void update_values(const char* json_text, std::map<std::string, std::string>* values)
{
    if (!json_text || !values)
        return;
    const nlohmann::json json = nlohmann::json::parse(json_text);
    if (!json.is_object())
        return;
    values->clear();
    for (const auto& [key, value] : json.items())
        (*values)[key] = value.is_string() ? value.get<std::string>() : value.dump();
}

void set_preset_business_error(int result, std::map<std::string, std::string>* values)
{
    if (values && result >= FNET_PRESET_UNSUPPORTED_TYPE && result <= FNET_PRESET_UPDATE_CONFLICT)
        (*values)["code"] = std::to_string(result);
}

} // namespace

std::string FlashCloudServiceAgent::generate_uuid_for_setting_id(
    const std::string& name, const std::string& type, const std::string& user_id)
{
    if (user_id.empty() || name.empty() || type.empty())
        return {};

    // Flash presets are uniquely identified by account, preset name and preset type.
    static const boost::uuids::uuid flash_namespace = boost::uuids::string_generator()("f47ac10b-58cc-4372-a567-0e02b2c3d479");
    boost::uuids::name_generator_sha1 generator(flash_namespace);
    const boost::uuids::uuid id = generator(user_id + "\n" + name + "\n" + type);
    return boost::uuids::to_string(id);
}

bool FlashCloudServiceAgent::is_user_login()
{
    auto* mgr = GUI::MultiComMgr::inst();
    if (!mgr || !mgr->isLoggedIn() || mgr->userId().empty() || !network_interface())
        return false;

    auto token = GUI::WanDevTokenMgr::inst()->getScopedToken();
    return !token.accessToken().empty();
}

std::string FlashCloudServiceAgent::get_user_id()
{
    auto* mgr = GUI::MultiComMgr::inst();
    return mgr && mgr->isLoggedIn() ? mgr->userId() : std::string();
}

int FlashCloudServiceAgent::user_logout(bool request)
{
    // FlashForge account lifecycle is owned by MultiComMgr and the GUI login flow.
    // Do not route logout through the unrelated BBL network plugin.
    (void) request;
    return -1;
}

std::string FlashCloudServiceAgent::get_access_token() const
{
    auto* mgr = GUI::MultiComMgr::inst();
    if (!mgr || !mgr->isLoggedIn())
        return {};

    auto token = GUI::WanDevTokenMgr::inst()->getScopedToken();
    return token.accessToken();
}

bool FlashCloudServiceAgent::ensure_token_fresh(const std::string& reason)
{
    (void) reason;
    auto* mgr = GUI::MultiComMgr::inst();
    if (!mgr || !mgr->isLoggedIn())
        return false;

    auto token = GUI::WanDevTokenMgr::inst()->getScopedToken();
    if (token.accessToken().empty())
        return false;
    return GUI::WanDevTokenMgr::inst()->refreshToken(token) == GUI::COM_OK;
}

int FlashCloudServiceAgent::get_user_presets(std::map<std::string, std::map<std::string, std::string>>* user_presets)
{
    if (!user_presets)
        return FNET_INVALID_PARAMETER;

    auto* intfc = network_interface();
    if (!intfc || !intfc->getUserPresets || !is_user_login())
        return FNET_ERROR;

    auto token = GUI::WanDevTokenMgr::inst()->getScopedToken();
    log_sync_json("getUserPresets", {{"userId", GUI::MultiComMgr::inst()->userId()}});
    char* presets_json = nullptr;
    const int result   = intfc->getUserPresets(GUI::MultiComMgr::inst()->clientId().c_str(), token.accessToken().c_str(), &presets_json);
    log_sync_result("getUserPresets", result, 0, presets_json);
    if (result != FNET_OK) {
        intfc->freeString(presets_json);
        return result;
    }

    try {
        user_presets->clear();

        const nlohmann::json presets = nlohmann::json::parse(presets_json ? presets_json : "{}");

        if (!presets.is_object())
            throw std::runtime_error("preset cache root is not an object");

        const std::string current_user_id = get_user_id();

        for (const auto& [cache_setting_id, preset] : presets.items()) {
            if (!preset.is_object())
                throw std::runtime_error("preset cache entry is not an object");

            const std::string preset_name = preset.value(BBL_JSON_KEY_NAME, "");
            const std::string preset_type = preset.value(BBL_JSON_KEY_TYPE, "");
            std::string setting_id        = preset.value(BBL_JSON_KEY_SETTING_ID, cache_setting_id);

            if (preset_name.empty() || preset_type.empty() || setting_id.empty())
                throw std::runtime_error("preset cache entry is missing name, type or setting_id");
            if (!cache_setting_id.empty() && setting_id != cache_setting_id)
                throw std::runtime_error("preset cache key does not match setting_id");

            const std::string preset_user_id = preset.value(BBL_JSON_KEY_USER_ID, current_user_id);
            if (!preset_user_id.empty() && preset_user_id != current_user_id)
                throw std::runtime_error("preset cache entry belongs to another user");

            std::map<std::string, std::string> value_map;
            for (const auto& [key, value] : preset.items()) {
                value_map[key] = value.is_string() ? value.get<std::string>() : value.dump();
            }

            value_map[BBL_JSON_KEY_NAME]       = preset_name;
            value_map[BBL_JSON_KEY_TYPE]       = preset_type;
            value_map[BBL_JSON_KEY_USER_ID]    = current_user_id;
            value_map[BBL_JSON_KEY_SETTING_ID] = setting_id;

            // FlashNetwork keys its cache by setting_id. Keep that identity here so
            // presets with the same name but different types cannot overwrite each other.
            const auto [unused, inserted] = user_presets->emplace(setting_id, std::move(value_map));
            if (!inserted)
                throw std::runtime_error("duplicate setting_id in preset cache");
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent: invalid preset cache JSON: " << e.what();

        intfc->freeString(presets_json);
        return FNET_GET_SETTING_LIST_FAILED;
    }

    intfc->freeString(presets_json);
    return FNET_OK;
}

std::string FlashCloudServiceAgent::request_setting_id(std::string name,
                                                       std::map<std::string, std::string>* values_map,
                                                       unsigned int* http_code)
{
    if (http_code)
        *http_code = 0;
    if (!values_map) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::request_setting_id failed: values_map is null";
        return {};
    }

    auto* intfc = network_interface();
    if (!intfc || !is_user_login()) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::request_setting_id failed: user is not logged in";
        return {};
    }
    if (!intfc->requestSettingId) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::request_setting_id failed: not in the loaded FlashNetwork library";
        return {};
    }

    const std::string user_id = get_user_id();
    const auto type_it        = values_map->find(BBL_JSON_KEY_TYPE);
    const std::string type    = type_it == values_map->end() ? std::string() : type_it->second;
    const std::string new_id  = generate_uuid_for_setting_id(name, type, user_id);
    if (new_id.empty()) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::request_setting_id failed: user_id, name or type is empty";
        return {};
    }

    auto token                      = GUI::WanDevTokenMgr::inst()->getScopedToken();
    nlohmann::json values           = values_to_json(values_map);
    values[BBL_JSON_KEY_SETTING_ID] = new_id;
    const std::string values_json   = values.dump();
    log_sync_json("requestSettingId", {{"userId", user_id}, {"name", name}, {"type", type}, {"values", values}});
    char* setting_id    = nullptr;
    char* result_values = nullptr;
    const int result    = intfc->requestSettingId(GUI::MultiComMgr::inst()->clientId().c_str(), token.accessToken().c_str(), name.c_str(),
                                                  values_json.c_str(), &setting_id, &result_values, http_code, PRESET_SYNC_TIMEOUT_MS);
    log_sync_result("requestSettingId", result, http_code ? *http_code : 0, result_values);

    std::string id;
    if (result == FNET_OK) {
        id = setting_id ? setting_id : "";
        try {
            update_values(result_values, values_map);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent: invalid create result JSON: " << e.what();
            id.clear();
        }
        if (!id.empty() && id != new_id) {
            BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent: server returned a different setting_id, expected=" << new_id
                                     << ", actual=" << id;
            id.clear();
        }
        if (!id.empty()) {
            (*values_map)[BBL_JSON_KEY_SETTING_ID] = new_id;
            (*values_map)[BBL_JSON_KEY_NAME]       = name;
            (*values_map)[BBL_JSON_KEY_TYPE]       = type;
            (*values_map)[BBL_JSON_KEY_USER_ID]    = user_id;
        }
    } else {
        set_preset_business_error(result, values_map);
    }
    intfc->freeString(setting_id);
    intfc->freeString(result_values);
    return id.empty() ? std::string() : new_id;
}

int FlashCloudServiceAgent::put_setting(
    std::string setting_id, std::string name, std::map<std::string, std::string>* values_map, unsigned int* http_code)
{
    if (http_code)
        *http_code = 0;
    if (!values_map || setting_id.empty() || name.empty()) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::put_setting failed: values_map, setting_id or name is invalid";
        return FNET_INVALID_PARAMETER;
    }

    // FlashForge resolves conflicts on the client and uses the same update endpoint
    // after the user has chosen the local version.
    auto* intfc = network_interface();
    if (!intfc || !is_user_login()) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::put_setting failed: user is not logged in";
        return FNET_ERROR;
    }
    if (!intfc->putSetting) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::put_setting failed: not in the loaded FlashNetwork library";
        return FNET_ERROR;
    }

    const auto type_it        = values_map->find(BBL_JSON_KEY_TYPE);
    const auto update_time_it = values_map->find(ORCA_JSON_KEY_UPDATE_TIME);
    if (type_it == values_map->end() || type_it->second.empty() || update_time_it == values_map->end() || update_time_it->second.empty()) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::put_setting failed: type or updated_time is missing";
        return FNET_INVALID_PARAMETER;
    }

    const auto local_id_it = values_map->find(BBL_JSON_KEY_SETTING_ID);
    if (local_id_it != values_map->end() && !local_id_it->second.empty() && local_id_it->second != setting_id) {
        BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent::put_setting failed: setting_id mismatch, expected=" << setting_id
                                 << ", actual=" << local_id_it->second;
        return FNET_INVALID_PARAMETER;
    }

    const std::string user_id = get_user_id();
    const std::string type    = type_it->second;
    auto token                = GUI::WanDevTokenMgr::inst()->getScopedToken();
    nlohmann::json values     = values_to_json(values_map);
    values[BBL_JSON_KEY_SETTING_ID] = setting_id;
    const std::string values_json = values.dump();
    log_sync_json("putSetting",
                  {{"userId", user_id},
                   {"settingId", setting_id},
                   {"name", name},
                   {"values", values}});
    char* result_values = nullptr;
    const int result    = intfc->putSetting(GUI::MultiComMgr::inst()->clientId().c_str(), token.accessToken().c_str(), setting_id.c_str(),
                                            name.c_str(), values_json.c_str(), &result_values, http_code, PRESET_SYNC_TIMEOUT_MS);
    log_sync_result("putSetting", result, http_code ? *http_code : 0, result_values);

    if (result == FNET_OK) {
        try {
            update_values(result_values, values_map);
            (*values_map)[BBL_JSON_KEY_SETTING_ID] = setting_id;
            (*values_map)[BBL_JSON_KEY_NAME]       = name;
            (*values_map)[BBL_JSON_KEY_TYPE]       = type;
            (*values_map)[BBL_JSON_KEY_USER_ID]    = user_id;
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "FlashCloudServiceAgent: invalid update result JSON: " << e.what();
            intfc->freeString(result_values);
            return FNET_PUT_SETTING_FAILED;
        }
    } else {
        set_preset_business_error(result, values_map);
    }
    intfc->freeString(result_values);
    return result;
}

int FlashCloudServiceAgent::get_setting_list(std::string bundle_version, ProgressFn pro_fn, WasCancelledFn cancel_fn)
{
    auto* intfc = network_interface();
    if (!intfc || !intfc->getSettingList || !is_user_login())
        return FNET_ERROR;

    auto token = GUI::WanDevTokenMgr::inst()->getScopedToken();
    log_sync_json("getSettingList", {{"userId", GUI::MultiComMgr::inst()->userId()}, {"bundleVersion", bundle_version}});
    SyncCallbacks callbacks{nullptr, &pro_fn, &cancel_fn};
    const int result = intfc->getSettingList(GUI::MultiComMgr::inst()->clientId().c_str(), token.accessToken().c_str(),
                                             bundle_version.c_str(), pro_fn ? progress_callback : nullptr, &callbacks,
                                             cancel_fn ? cancel_callback : nullptr, &callbacks, PRESET_SYNC_TIMEOUT_MS);
    log_sync_result("getSettingList", result, 0);
    return result;
}

int FlashCloudServiceAgent::get_setting_list2(std::string bundle_version, CheckFn chk_fn, ProgressFn pro_fn, WasCancelledFn cancel_fn)
{
    auto* intfc = network_interface();
    if (!intfc || !intfc->getSettingList2 || !is_user_login())
        return FNET_ERROR;

    auto token = GUI::WanDevTokenMgr::inst()->getScopedToken();
    log_sync_json("getSettingList2", {{"userId", GUI::MultiComMgr::inst()->userId()}, {"bundleVersion", bundle_version}});
    SyncCallbacks callbacks{&chk_fn, &pro_fn, &cancel_fn};
    const int result = intfc->getSettingList2(GUI::MultiComMgr::inst()->clientId().c_str(), token.accessToken().c_str(),
                                              bundle_version.c_str(), chk_fn ? check_callback : nullptr, &callbacks,
                                              pro_fn ? progress_callback : nullptr, &callbacks, cancel_fn ? cancel_callback : nullptr,
                                              &callbacks, PRESET_SYNC_TIMEOUT_MS);
    log_sync_result("getSettingList2", result, 0);
    return result;
}

int FlashCloudServiceAgent::delete_setting(std::string setting_id)
{
    if (setting_id.empty())
        return FNET_INVALID_PARAMETER;

    auto* intfc = network_interface();
    if (!intfc || !intfc->deleteSetting || !is_user_login())
        return FNET_ERROR;

    auto token             = GUI::WanDevTokenMgr::inst()->getScopedToken();
    unsigned int http_code = 0;
    log_sync_json("deleteSetting", {{"userId", GUI::MultiComMgr::inst()->userId()}, {"settingId", setting_id}});
    const int result = intfc->deleteSetting(GUI::MultiComMgr::inst()->clientId().c_str(), token.accessToken().c_str(), setting_id.c_str(),
                                            &http_code, PRESET_SYNC_TIMEOUT_MS);
    log_sync_result("deleteSetting", result, http_code);
    return result;
}

} // namespace Slic3r