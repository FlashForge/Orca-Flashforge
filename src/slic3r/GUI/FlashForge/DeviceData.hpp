#ifndef slic3r_DeviceData_hpp_
#define slic3r_DeviceData_hpp_

#include "nlohmann/json.hpp"
#include "FlashNetwork.h"
#include "MultiComEvent.hpp"

using namespace nlohmann;  // json open source library
using namespace std;

namespace Slic3r {

namespace GUI {

#define CONNECTTYPE_LAN "lan"
#define CONNECTTYPE_CLOUD "cloud"

enum ConnectMode {
    UNKNOW_MODE = -1,
    LAN_MODE,
    WAN_MODE
};

enum ActiveState {
    NotActive, 
    Active, 
    UpdateToDate
};

struct device_wan_info
{
    string name;
    string bind_dev_id;
    int pid;
    string serialNum;
};

class DeviceObject
{
public:
    DeviceObject(const string& dev_id, const string& dev_name);
    DeviceObject(const fnet_lan_dev_info &devInfo);
    DeviceObject(const device_wan_info &wanInfo);
    ~DeviceObject();

    bool        is_lan_mode_in_scan_print();
    bool        is_lan_mode_printer();
    bool        has_access_right();

    void        set_user_access_code(const string& code, bool only_refresh = true);
    string      get_user_access_code(bool inner = false);
    void        erase_user_access_code();

    bool        is_avaliable();

    void        set_online_state(bool on_off);
    bool        is_online();

    void        set_active_state(ActiveState state);
    ActiveState get_active_state();

    void        set_connection_type(const string& connectType);
    string      connection_type();

    void        reset_update_time();

    fnet_lan_dev_info *get_lan_dev_info();
    string      get_dev_name();
    string      get_dev_id();  // serialNumber
    unsigned short     get_dev_pid();
    string      get_wan_dev_id();

    static bool is_in_printing_status(const string& status);
    void        set_print_state(const string &status);

    /* common apis */
    bool        is_in_printing();

    void        set_connecting(bool connecting);
    bool        is_connecting();

    void        set_connected_ready(bool ready);
    bool        is_connected_ready();

    string      get_printer_thumbnail_img_str();

private:
    fnet_lan_dev_info *m_lan_info { nullptr };
    device_wan_info   *m_wan_info { nullptr };
    string             m_dev_id;
    string             m_dev_name;

    string              m_user_access_code;
    string              m_bind_state; /* free | occupied */
    ActiveState         m_active_state = NotActive; // 0 - not active, 1 - active, 2 - update-to-date
    bool                m_is_online;
    bool                m_is_connecting { false };
    bool                m_is_connected_ready { true };
    string              m_dev_connection_type; /* lan | cloud */

    std::chrono::system_clock::time_point last_update_time; /* last received print data from machine */

    /* printing status */
    string m_printStatus; /* enum string: FINISH, SLICING, RUNNING, PAUSE, INIT, FAILED */
};

typedef std::map<std::string, std::string> MacInfoMap;

class DeviceObjectOpr
{
public:
    DeviceObjectOpr();
    ~DeviceObjectOpr();

public:
    void update_scan_machine();
    void read_local_machine_from_config();

    void get_local_machine(map<string, DeviceObject *>& macList);

    bool set_selected_machine(const string &dev_id);
    DeviceObject* get_selected_machine();

    void unbind_lan_machine(DeviceObject *obj);
    ComErrno unbind_wan_machine(DeviceObject *obj);

    /* return machine has access code and user machine if login*/
    void get_my_machine_list(map<string, DeviceObject *> &devList);

private:
    DeviceObject *get_scan_device(const string &dev_id);

    // before connect, scan machine's access code which hasn't written in config file
    void get_my_machine_list_v2(map<string, DeviceObject *> &devList);

    void clear_scan_machine();
    
    string find_dev_id_from_connection(int connectId);

private:
    void onConnectExit(ComConnectionExitEvent &event);
    void onConnectReady(ComConnectionReadyEvent &event);

private:
    string                            m_selected_machine;       /* dev_id */
    map<string, DeviceObject *>       m_scan_devices;       /* dev_id -> DeviceObject*, scan in lan (only lan connectMode, and wan connectMode)   */
    map<string, DeviceObject *>       m_user_devices;        /* dev_id -> DeviceObject*, when user login, the user's devices that has bound. And machine connected successfully. */
    map<string, DeviceObject*>        m_local_devices;      /* dev_id -> DeviceObject*,  in lan connectMode, device has input access code. Read data from appconfig. */
    map<string, com_id_t>             m_dev_connect_map;   /* dev_id -> connectId */
};

}

}



#endif