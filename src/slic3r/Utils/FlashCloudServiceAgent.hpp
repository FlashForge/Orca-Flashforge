#ifndef __FLASH_CLOUD_SERVICE_AGENT_HPP__
#define __FLASH_CLOUD_SERVICE_AGENT_HPP__

#include "ICloudServiceAgent.hpp"

namespace Slic3r {

class FlashCloudServiceAgent final : public ICloudServiceAgent
{
public:
    FlashCloudServiceAgent() = default;
    ~FlashCloudServiceAgent() override = default;

    // FlashForge builds use NetworkAgent's primary cloud slot, currently keyed as Orca.
    std::string get_id() const override { return ORCA_CLOUD_PROVIDER; }

    static std::string generate_uuid_for_setting_id(const std::string &name, const std::string &type,
                                                    const std::string &user_id);
    bool is_user_login() override;
    std::string get_user_id() override;

    int init_log() override { return 0; }
    int set_config_dir(std::string) override { return 0; }
    int set_cert_file(std::string, std::string) override { return 0; }
    int set_country_code(std::string) override { return 0; }
    int start() override { return 0; }
    int change_user(std::string) override { return -1; }
    int user_logout(bool request = false) override;
    std::string get_user_name() override { return {}; }
    std::string get_user_avatar() override { return {}; }
    std::string get_user_nickname() override { return {}; }
    std::string build_login_cmd() override { return {}; }
    std::string build_logout_cmd() override { return {}; }
    std::string build_login_info() override { return {}; }
    std::string get_access_token() const override;
    std::string get_refresh_token() const override { return {}; }
    bool ensure_token_fresh(const std::string &reason) override;
    std::string get_cloud_service_host() override { return {}; }
    std::string get_cloud_login_url(const std::string & = "") override { return {}; }
    int connect_server() override { return is_user_login() ? 0 : -1; }
    bool is_server_connected() override { return is_user_login(); }
    // FlashForge WAN connection recovery is owned by MultiComMgr. This generic
    // device heartbeat runs every five seconds, so it must not rotate OAuth tokens.
    int refresh_connection() override { return is_user_login() ? 0 : -1; }
    int start_subscribe(std::string) override { return -1; }
    int stop_subscribe(std::string) override { return -1; }
    int add_subscribe(std::vector<std::string>) override { return -1; }
    int del_subscribe(std::vector<std::string>) override { return -1; }
    void enable_multi_machine(bool) override {}

    int get_user_presets(std::map<std::string, std::map<std::string, std::string>> *user_presets) override;
    std::string request_setting_id(std::string name, std::map<std::string, std::string> *values_map,
                                   unsigned int *http_code) override;
    int put_setting(std::string setting_id, std::string name, std::map<std::string, std::string> *values_map,
                    unsigned int *http_code) override;
    int get_setting_list(std::string bundle_version, ProgressFn pro_fn = nullptr,
                         WasCancelledFn cancel_fn = nullptr) override;
    int get_setting_list2(std::string bundle_version, CheckFn chk_fn, ProgressFn pro_fn = nullptr,
                          WasCancelledFn cancel_fn = nullptr) override;
    int delete_setting(std::string setting_id) override;

    int get_my_message(int, int, int, unsigned int *, std::string *) override { return -1; }
    int check_user_task_report(int *, bool *) override { return -1; }
    int get_user_print_info(unsigned int *, std::string *) override { return -1; }
    int get_user_tasks(TaskQueryParams, std::string *) override { return -1; }
    int get_printer_firmware(std::string, unsigned *, std::string *) override { return -1; }
    int get_task_plate_index(std::string, int *) override { return -1; }
    int get_user_info(int *) override { return -1; }
    int get_subtask_info(std::string, std::string *, unsigned int *, std::string *) override { return -1; }
    int get_slice_info(std::string, std::string, int, std::string *) override { return -1; }
    int query_bind_status(std::vector<std::string>, unsigned int *, std::string *) override { return -1; }
    int modify_printer_name(std::string, std::string) override { return -1; }
    int get_camera_url(std::string, std::function<void(std::string)>) override { return -1; }
    int get_design_staffpick(int, int, std::function<void(std::string)>) override { return -1; }
    int start_publish(PublishParams, OnUpdateStatusFn, WasCancelledFn, std::string *) override { return -1; }
    int get_model_publish_url(std::string *) override { return -1; }
    int get_subtask(BBLModelTask *, OnGetSubTaskFn) override { return -1; }
    int get_model_mall_home_url(std::string *) override { return -1; }
    int get_model_mall_detail_url(std::string *, std::string) override { return -1; }
    int get_my_profile(std::string, unsigned int *, std::string *) override { return -1; }
    int get_my_token(std::string, unsigned int *, std::string *) override { return -1; }
    int track_enable(bool) override { return -1; }
    int track_remove_files() override { return -1; }
    int track_event(std::string, std::string) override { return -1; }
    int track_header(std::string) override { return -1; }
    int track_update_property(std::string, std::string, std::string = "string") override { return -1; }
    int track_get_property(std::string, std::string &, std::string = "string") override { return -1; }
    bool get_track_enable() override { return false; }
    int put_model_mall_rating(int, int, std::string, std::vector<std::string>, unsigned int &, std::string &) override { return -1; }
    int get_oss_config(std::string &, std::string, unsigned int &, std::string &) override { return -1; }
    int put_rating_picture_oss(std::string &, std::string &, std::string, int, unsigned int &, std::string &) override { return -1; }
    int get_model_mall_rating_result(int, std::string &, unsigned int &, std::string &) override { return -1; }
    int get_mw_user_preference(std::function<void(std::string)>) override { return -1; }
    int get_mw_user_4ulist(int, int, std::function<void(std::string)>) override { return -1; }
    std::string get_version() override { return {}; }
    int set_on_server_connected_fn(AppOnServerConnectedFn) override { return 0; }
    int set_on_http_error_fn(AppOnHttpErrorFn) override { return 0; }
    int set_get_country_code_fn(GetCountryCodeFn) override { return 0; }
    int set_queue_on_main_fn(QueueOnMainFn) override { return 0; }
};

} // namespace Slic3r

#endif
