#include "DeviceData.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "MultiComMgr.hpp"
#include "slic3r/GUI/ConnectPrinter.hpp"
#include "slic3r/GUI/MainFrame.hpp"

namespace Slic3r {

namespace GUI {


DeviceObject::DeviceObject(const std::string& dev_id, const std::string& dev_name)
    : m_lan_info(nullptr)
    , m_dev_id(dev_id)
    , m_dev_name(dev_name)
    , m_is_online(false)
{
    m_bind_state = "free";
    m_deviceType = DT_LOCAL;
}

DeviceObject::DeviceObject(const fnet_lan_dev_info &devInfo)
    :m_is_online(false)
{
    m_lan_info   = new fnet_lan_dev_info(devInfo);
    m_dev_id     = m_lan_info->serialNumber;
    m_dev_name   = m_lan_info->name;
    m_bind_state = "free";
    m_deviceType = DT_LOCAL;
}

DeviceObject::DeviceObject(const device_wan_info &wanInfo)
    : m_lan_info(nullptr)
    , m_is_online(true)
{
    m_wan_info = new device_wan_info(wanInfo);
    m_dev_id = m_wan_info->serialNum;
    m_dev_name   = m_wan_info->name;
    m_bind_state = "free";
    m_deviceType = DT_USER;
}

DeviceObject::~DeviceObject()
{
    if (m_lan_info != nullptr) {
        delete m_lan_info;
        m_lan_info = nullptr;
    }

    if (m_wan_info != nullptr) {
        delete m_wan_info;
        m_wan_info = nullptr;
    }
}

bool DeviceObject::is_lan_mode_in_scan_print()
{
    if (m_lan_info == nullptr)
        return true;
    else {
        if (m_lan_info->connectMode == 0)
            return true;
    }
    return false;
}

bool DeviceObject::is_lan_mode_printer()
{
    if (m_dev_connection_type.empty())
        return true;
    else {
        if (m_dev_connection_type == CONNECTTYPE_LAN)
            return true;
    }
    return false;
}

bool DeviceObject::has_access_right()
{
    return !get_user_access_code().empty();
}

void DeviceObject::set_user_access_code(const std::string& code, bool only_refresh /* = true*/)
{
    if (code.empty()) {
        m_user_access_code = " ";
        return;
    }
    m_user_access_code = code;
    if (only_refresh && !code.empty()) {
        AppConfig *config = GUI::wxGetApp().app_config;
        if (config) {
            GUI::wxGetApp().app_config->set_str("user_access_code", m_dev_id, code);
        }
    }
}

std::string DeviceObject::get_user_access_code(bool inner /* = false*/)
{
    if (inner)
        return m_user_access_code;

    AppConfig *config = GUI::wxGetApp().app_config;
    if (config) {
        return GUI::wxGetApp().app_config->get("user_access_code", m_dev_id);
    }
    return "";
}

void DeviceObject::erase_user_access_code()
{
    m_user_access_code = "";
    AppConfig *config      = GUI::wxGetApp().app_config;
    if (config) {
        GUI::wxGetApp().app_config->erase("user_access_code", m_dev_id);
        GUI::wxGetApp().app_config->save();
    }
}

bool DeviceObject::is_avaliable()
{
    return m_bind_state == "free";
}

void DeviceObject::set_online_state(bool on_off)
{
    m_is_online = on_off;
    if (!on_off)
        m_active_state = NotActive;
}

bool DeviceObject::is_online()
{
    return m_is_online;
}

void DeviceObject::set_active_state(ActiveState state)
{
    m_active_state = state;
}

ActiveState DeviceObject::get_active_state()
{
    return m_active_state;
}

void DeviceObject::set_connection_type(const std::string& connectType) {
    m_dev_connection_type = connectType;
}

std::string DeviceObject::connection_type() {
    return m_dev_connection_type; 
}

void DeviceObject::reset_update_time()
{
    BOOST_LOG_TRIVIAL(trace) << "reset reset_update_time, dev_id =" << m_dev_id;
    last_update_time = std::chrono::system_clock::now();
}

fnet_lan_dev_info * DeviceObject::get_lan_dev_info()
{
    return m_lan_info;
}

//void DeviceObject::set_lan_dev_info(fnet_lan_dev_info *info) 
//{ 
//    m_lan_info = info;
//}

void DeviceObject::set_lan_dev_info(const fnet_lan_dev_info &info) 
{ 
    if (m_lan_info != nullptr) {
        m_lan_info->bindStatus = info.bindStatus;
        m_lan_info->connectMode = info.connectMode;
        m_lan_info->pid         = info.pid;
        m_lan_info->port        = info.port;
        m_lan_info->vid         = info.vid;
        strcpy(m_lan_info->ip, info.ip);
        strcpy(m_lan_info->serialNumber, info.serialNumber);
        strcpy(m_lan_info->name, info.name);
    } else {
        m_lan_info = new fnet_lan_dev_info(info);
    }
    //m_lan_info = info; 
}

void DeviceObject::set_wan_dev_info(const device_wan_info &info)
{
    if (m_wan_info != nullptr) {
        m_wan_info->bind_dev_id = info.bind_dev_id;
        m_wan_info->nim_account_id = info.nim_account_id;
        m_wan_info->name        = info.name;
        m_wan_info->pid         = info.pid;
        m_wan_info->serialNum   = info.serialNum;
    } else {
        m_wan_info = new device_wan_info(info);
    }
}

void DeviceObject::init_lan_obj()
{
    if (m_lan_info == nullptr)
        return;
    m_dev_id     = m_lan_info->serialNumber;
    m_dev_name   = m_lan_info->name;
    m_bind_state = "free";
    m_deviceType = DT_LOCAL;
}

void DeviceObject::init_wan_obj()
{
    if (m_wan_info == nullptr)
        return;
    m_dev_id     = m_wan_info->serialNum;
    m_dev_name   = m_wan_info->name;
    m_bind_state = "free";
    m_deviceType = DT_USER;
}

std::string DeviceObject::get_dev_name() {
    return m_dev_name;
}

void DeviceObject::set_dev_name(const std::string& name) { 
    m_dev_name = name; 
}

std::string DeviceObject::get_dev_ip() {
    if (m_lan_info != nullptr) {
        return m_lan_info->ip;
    }
    return std::string();
}

unsigned short DeviceObject::get_dev_port() {
    if (m_lan_info != nullptr) {
        return m_lan_info->port;
    }
    return 0;
}

std::string DeviceObject::get_dev_id() {
    return m_dev_id;
}

unsigned short DeviceObject::get_dev_pid()
{
    if (m_lan_info != nullptr)
        return m_lan_info->pid;
    else if (m_wan_info != nullptr)
        return m_wan_info->pid;
    return 0;
}

std::string DeviceObject::get_wan_dev_id()
{
    if(m_wan_info == nullptr)
        return "";
    return m_wan_info->bind_dev_id;
}

std::string DeviceObject::get_wan_nim_account_id()
{
    if (m_wan_info == nullptr) {
        return "";
    }
    return m_wan_info->nim_account_id;
}

bool DeviceObject::is_in_printing_status(const std::string& status)
{
    if (status.compare("PAUSE") == 0 || status.compare("RUNNING") == 0 || status.compare("SLICING") == 0 || status.compare("PREPARE") == 0) {
        return true;
    }
    return false;
}

void DeviceObject::set_print_state(const std::string& status) {
    m_printStatus = status;
}

bool DeviceObject::is_in_printing()
{
    /* use print_status if print_status is valid */
    if (!m_printStatus.empty())
        return DeviceObject::is_in_printing_status(m_printStatus);
    /*else {
        return DeviceObject::is_in_printing_status(iot_print_status);
    }*/
    return false;
}

void DeviceObject::set_connecting(bool connecting)
{
    m_is_connecting = connecting;
}

bool DeviceObject::is_connecting()
{
    return m_is_connecting;
}

void DeviceObject::set_connected_ready(bool ready)
{
    m_is_connected_ready = ready;
}

bool DeviceObject::is_connected_ready()
{
    return m_is_connected_ready;
}

std::string DeviceObject::get_printer_thumbnail_img_str() {
    return "printer_thumbnail";
}

void DeviceObject::set_device_type(DeviceType type) 
{ 
    m_deviceType = type;
}

DeviceType DeviceObject::device_type() 
{ 
    return m_deviceType;
}

int DeviceObject::connectMode() 
{ 
    if (m_lan_info == nullptr)
        return 0;
    return m_lan_info->connectMode; 
}

BindInfo* DeviceObject::get_bind_info() 
{ 
    BindInfo* info = new BindInfo();
    info->dev_id   = get_dev_id();
    info->bind_id  = get_wan_dev_id();
    info->nim_account_id = get_wan_nim_account_id();
    info->dev_ip   = get_dev_ip();
    info->dev_port = get_dev_port();
    info->dev_name = get_dev_name();
    info->dev_pid  = get_dev_pid();
    info->img      = get_printer_thumbnail_img_str();
    return info;
}


wxDEFINE_EVENT(EVT_DEVICE_LIST_UPDATED, DeviceListUpdateEvent);
wxDEFINE_EVENT(EVT_LOCAL_DEVICE_NAME_CHANGED, LocalDeviceNameChangeEvent);
DeviceObjectOpr::DeviceObjectOpr()
{
    read_local_machine_from_config();
    MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &DeviceObjectOpr::onConnectExit, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &DeviceObjectOpr::onConnectReady, this);
    MultiComMgr::inst()->Bind(COM_WAN_DEV_INFO_UPDATE_EVENT, &DeviceObjectOpr::onConnectWanDevInfoUpdate, this);
}

DeviceObjectOpr::~DeviceObjectOpr()
{
    clear_scan_machine();
    clear_user_machine(); 

    for (auto it = m_local_devices.begin(); it != m_local_devices.end(); it++) {
        if (it->second) {
            delete it->second;
            it->second = nullptr;
        }
    }
    m_local_devices.clear();
}

void DeviceObjectOpr::update_scan_machine()
{
    //clear_scan_machine();

    std::vector<fnet_lan_dev_info> devInfos;
    MultiComUtils::getLanDevList(devInfos);
    update_scan_list(devInfos);
    for (auto &elem : devInfos) {
        std::string dev_id = elem.serialNumber;
        #if 1
        if (elem.connectMode == 1 && elem.bindStatus == 1) {
            auto it = m_local_devices.find(dev_id);
            if (it != m_local_devices.end()) {
                std::string name = elem.name;
                // auto lanInfo = new fnet_lan_dev_info(*info);
                if (name != it->second->get_dev_name()) {
                    it->second->set_dev_name(name);
                    AppConfig* config = GUI::wxGetApp().app_config;
                    if (config) {
                        config->save_bind_machine_to_config(dev_id, name, "", elem.pid,false);
                    }
                    LocalDeviceNameChangeEvent event(EVT_LOCAL_DEVICE_NAME_CHANGED, dev_id, name);
                    event.SetEventObject(this);
                    wxPostEvent(this, event);
                }
            }
            continue;
        }
            
        #endif
        bool          newObj = false;
        DeviceObject *devObj = nullptr;
        auto   scanIt = m_scan_devices.find(dev_id);
        if (scanIt != m_scan_devices.end()) {
            devObj       = scanIt->second;
            //auto lanInfo = new fnet_lan_dev_info(elem);
            devObj->set_lan_dev_info(elem);
            devObj->init_lan_obj();
        } else {
            scanIt = m_old_devices.find(dev_id);
            if (scanIt != m_old_devices.end()) {
                devObj       = scanIt->second;
                //auto lanInfo = new fnet_lan_dev_info(elem);
                devObj->set_lan_dev_info(elem);
                devObj->init_lan_obj();
                m_old_devices.erase(scanIt);
                newObj = true;
            } else {
                devObj = new DeviceObject(elem);
                newObj = true;
            }
        }
        devObj->set_connection_type(CONNECTTYPE_LAN);

        //DeviceObject *devObj = new DeviceObject(elem);
        //devObj->set_connection_type(CONNECTTYPE_LAN);
        auto it = m_local_devices.find(dev_id);
        if (it != m_local_devices.end()) {
            devObj->set_user_access_code(it->second->get_user_access_code(), false);
            auto info    = devObj->get_lan_dev_info();
            std::string name = info->name;
            //auto lanInfo = new fnet_lan_dev_info(*info);
            info->connectMode = 0;            
            it->second->set_lan_dev_info(*info);
            if (name != it->second->get_dev_name()) {
                it->second->set_dev_name(name);
                AppConfig* config = GUI::wxGetApp().app_config;
                if (config) {
                    config->save_bind_machine_to_config(dev_id, info->name, "", info->pid);
                }
                LocalDeviceNameChangeEvent event(EVT_LOCAL_DEVICE_NAME_CHANGED, dev_id, name);
                event.SetEventObject(this);
                wxPostEvent(this, event);
            }
        } else {
            it = m_user_devices.find(dev_id);
            if (it != m_user_devices.end()) {
                devObj->set_user_access_code(it->second->get_user_access_code(), false);
            }
        }
        if (newObj)
            m_scan_devices.insert(make_pair(dev_id, devObj));
    }
}

// connect_type: lan
void DeviceObjectOpr::read_local_machine_from_config()
{
    AppConfig *config = GUI::wxGetApp().app_config;
    if (config) {
        std::vector<MacInfoMap> macInfo;
        config->get_local_mahcines(macInfo);

        for (auto& mac : macInfo) {
            auto it = mac.find("dev_id");
            if (it != mac.end()) {
                std::string dev_id = it->second;
                std::string dev_name;
                it = mac.find("dev_name");
                if (it != mac.end())
                    dev_name = it->second;
                if (m_local_devices.find(dev_id) == m_local_devices.end()) {
                    DeviceObject *obj = new DeviceObject(dev_id, dev_name);
                    std::string   code = GUI::wxGetApp().app_config->get("user_access_code", dev_id);
                    if (!code.empty())
                        obj->set_user_access_code(code, false);
                    m_local_devices.emplace(dev_id, obj);
                }
            }
        }
    }
}

void DeviceObjectOpr::get_local_machine(map<std::string, DeviceObject*>& macList)
{
    macList.clear();
    macList.insert(m_local_devices.begin(), m_local_devices.end());
}

void DeviceObjectOpr::get_scan_machine(std::map<std::string, DeviceObject*>& macList)
{
    macList.clear();
    macList.insert(m_scan_devices.begin(), m_scan_devices.end());
}

void DeviceObjectOpr::get_user_machine(std::map<std::string, DeviceObject*>& macList)
{
    macList.clear();
    macList.insert(m_user_devices.begin(), m_user_devices.end());
}

bool DeviceObjectOpr::my_machine_empty()
{
    if (m_user_devices.empty() && m_local_devices.empty())
        return true;
    return false;
}

bool DeviceObjectOpr::set_selected_machine(const std::string& dev_id, bool my_machine /*= false*/)
{
    BOOST_LOG_TRIVIAL(info) << "set_selected_machine begin" << dev_id;
    flush_logs();
    map<std::string, DeviceObject*> my_machine_list;
    get_my_machine_list_v2(my_machine_list, my_machine);
    auto it = my_machine_list.find(dev_id);

    if (it != my_machine_list.end()) {
        DeviceObject *devObj = it->second;
        if (devObj->get_lan_dev_info() != nullptr) {
            devObj->set_connecting(true);
            com_id_t id = MultiComMgr::inst()->addLanDev(*devObj->get_lan_dev_info(), devObj->get_user_access_code(true));
            if (id != ComInvalidId) {
                 auto it = m_lan_dev_connect_map.find(dev_id);
                if (it != m_lan_dev_connect_map.end())
                     m_lan_dev_connect_map.erase(it);
                id_connect_mode mode;
                mode.id = id;
                mode.mode = COM_CONNECT_LAN;
                m_lan_dev_connect_map.emplace(make_pair(dev_id, mode));
            }
        }

        m_selected_machine = dev_id;
    }
    BOOST_LOG_TRIVIAL(info) << "set_selected_machine end";
    flush_logs();
    return true;
}

DeviceObject *DeviceObjectOpr::get_selected_machine()
{
    if (m_selected_machine.empty())
        return nullptr;

    // get data from userData, if not found, then find in scan machine data
    auto it = m_user_devices.find(m_selected_machine);
    if (it != m_user_devices.end()) {
        return it->second;
    } else {
        // return local machine has access code
        it = m_local_devices.find(m_selected_machine);
        if (it != m_local_devices.end()) {
            if (it->second->has_access_right())
                return it->second;
        } else {
            it = m_scan_devices.find(m_selected_machine);
            if (it != m_scan_devices.end())
                return it->second;
        }
    }
    return nullptr;
}

void DeviceObjectOpr::unbind_lan_machine(DeviceObject *obj)
{
    if (obj == nullptr) {
        return;
    }
    std::string dev_id = obj->get_dev_id();
    obj->erase_user_access_code();
    AppConfig *config = GUI::wxGetApp().app_config;
    if (config) {
        config->erase_local_machine(dev_id, obj->get_dev_name());
    }
    auto     it = m_lan_dev_connect_map.find(dev_id);
    com_id_t id = -1;
    if (it != m_lan_dev_connect_map.end()) {
        id = it->second.id;
        m_lan_dev_connect_map.erase(it);
    }
    auto devIt = m_local_devices.find(dev_id);
    if (devIt != m_local_devices.end()) {
        delete devIt->second;
        devIt->second = nullptr;
        m_local_devices.erase(devIt);
    }
    MultiComMgr::inst()->removeLanDev(id);
    sendDeviceListUpdateEvent(dev_id, -1);
}

ComErrno DeviceObjectOpr::unbind_wan_machine(const std::string& dev_id, const std::string& bind_id,
    const std::string& nim_account_id)
{
    ComErrno ret = MultiComMgr::inst()->unbindWanDev(dev_id, bind_id, nim_account_id);
    if (ret == COM_OK) {
        auto it = m_wan_dev_connect_map.find(dev_id);
        if (it != m_wan_dev_connect_map.end()) {
            m_wan_dev_connect_map.erase(it);
        }
        auto devIt = m_user_devices.find(dev_id);
        if (devIt != m_user_devices.end()) {
            //delete devIt->second;
            //devIt->second = nullptr;
            auto old_it   = m_old_user_devices.find(dev_id);
            if (old_it == m_old_user_devices.end()) {
                m_old_user_devices.emplace(make_pair(dev_id, devIt->second));
            }
            m_user_devices.erase(devIt);
        }
        sendDeviceListUpdateEvent(dev_id, -1);
    } else {
        BOOST_LOG_TRIVIAL(info) << "unbindWanDev failed: " << dev_id;
    }
    return ret;
}

void DeviceObjectOpr::removeUserDev(DeviceObject *obj) 
{
    std::string dev_id = obj->get_dev_id();
    auto it = m_wan_dev_connect_map.find(dev_id);
    if (it != m_wan_dev_connect_map.end()) {
        m_wan_dev_connect_map.erase(it);
    }
    auto devIt = m_user_devices.find(dev_id);
    if (devIt != m_user_devices.end()) {
        //delete devIt->second;
        //devIt->second = nullptr;
        auto old_it   = m_old_user_devices.find(dev_id);
        if (old_it == m_old_user_devices.end()) {
            m_old_user_devices.emplace(make_pair(dev_id, devIt->second));
        }
        m_user_devices.erase(devIt);
    }
    sendDeviceListUpdateEvent(dev_id, -1);
}


void DeviceObjectOpr::get_my_machine_list(map<std::string, DeviceObject*>& devList)
{
    devList.clear();
    devList.insert(m_user_devices.begin(), m_user_devices.end());

#if 0
    for (auto it = m_scan_devices.begin(); it != m_scan_devices.end(); it++) {
        if (!it->second)
            continue;
        if (it->second->has_access_right()) {
            // remove redundant in userMachineList
            if (devList.find(it->first) == devList.end()) {
                devList.emplace(make_pair(it->first, it->second));
            }
        }
    }
#endif

    for (auto it = m_local_devices.begin(); it != m_local_devices.end(); it++) {
        auto tmpIt = devList.find(it->first);
        if (tmpIt == devList.end()) {
            devList.emplace(make_pair(it->first, it->second));
        } else {
            if (tmpIt->second->device_type() == DT_BOTH && !tmpIt->second->is_online()) {
                devList.erase(tmpIt);
                devList.emplace(make_pair(it->first, it->second));
            }
        }
    }
}

void DeviceObjectOpr::clear_user_machine()
{
    for (auto it = m_user_devices.begin(); it != m_user_devices.end();) {
        if (!it->second->is_lan_mode_printer()) {
            delete it->second;
            it->second = nullptr;
            m_user_devices.erase(it++);
        } else it++;
    }
    for (auto it = m_old_user_devices.begin(); it != m_old_user_devices.end();) {
        if (!it->second->is_lan_mode_printer()) {
            delete it->second;
            it->second = nullptr;
            m_old_user_devices.erase(it++);
        } else it++;
    }
}

DeviceObject* DeviceObjectOpr::get_scan_device(const std::string& dev_id)
{
    if (dev_id.empty())
        return nullptr;
    auto it = m_scan_devices.find(dev_id);
    if (it == m_scan_devices.end()) {
        it = m_user_devices.find(dev_id);
        if (it == m_user_devices.end())
            return nullptr;
    }
    return it->second;
}

void DeviceObjectOpr::get_my_machine_list_v2(map<std::string, DeviceObject*>& devList, bool my_machine /* = false*/)
{
    if (!my_machine) {
        for (auto it = m_scan_devices.begin(); it != m_scan_devices.end(); it++) {
            if (!it->second)
                continue;
            if (!it->second->get_user_access_code(true).empty()) {
                // remove redundant in userMachineList
                if (devList.find(it->first) == devList.end()) {
                    devList.emplace(make_pair(it->first, it->second));
                }
            }
        }
    }    

    for (auto it = m_local_devices.begin(); it != m_local_devices.end(); it++) {
        if (devList.find(it->first) == devList.end()) {
            devList.emplace(make_pair(it->first, it->second));
        }
    }
}

void DeviceObjectOpr::clear_scan_machine()
{
    for (auto it = m_scan_devices.begin(); it != m_scan_devices.end(); it++) {
        if (it->second) {
            delete it->second;
            it->second = nullptr;
        }
    }
    m_scan_devices.clear();
    for (auto it = m_old_devices.begin(); it != m_old_devices.end(); it++) {
        if (it->second) {
            delete it->second;
            it->second = nullptr;
        }
    }
    m_old_devices.clear();
}

void DeviceObjectOpr::update_scan_list(const std::vector<fnet_lan_dev_info> &infos)
{
    map<std::string, fnet_lan_dev_info> lan_map;
    for (auto info : infos){
#if 1
        if (info.connectMode == 1 && info.bindStatus == 1)
            continue;
#endif
        lan_map.emplace(make_pair(info.serialNumber, info));
    }
    for (auto it = m_scan_devices.begin(); it != m_scan_devices.end();) {
        if (it->second) {
            it->second->set_connecting(false);
            auto tmpIt = lan_map.find(it->second->get_dev_id());
            if (tmpIt == lan_map.end()) {
                auto oldIt = m_old_devices.find(it->second->get_dev_id());
                if (oldIt == m_old_devices.end()) {
                    m_old_devices.emplace(make_pair(it->second->get_dev_id(), it->second));
                }
                m_scan_devices.erase(it++);
            } else{
                it++;
            }
        } else{
            it++;
        }
    }
}

std::string DeviceObjectOpr::find_dev_from_id(id_connect_mode& mode, int connectId)
{
    for (auto it = m_lan_dev_connect_map.begin(); it != m_lan_dev_connect_map.end(); ++it) {
        if (it->second.id == connectId) {
            mode = it->second;
            return it->first;
        }
    }

    for (auto it = m_wan_dev_connect_map.begin(); it != m_wan_dev_connect_map.end(); ++it) {
        if (it->second.id == connectId) {
            mode = it->second;
            return it->first;
        }
    }
    return "";
}

void DeviceObjectOpr::sendDeviceListUpdateEvent(const std::string& dev_id, int conn_id, bool wan_offline/*=false*/)
{
    DeviceListUpdateEvent event(EVT_DEVICE_LIST_UPDATED);
    event.SetDeviceId(dev_id);
    bool update_flag = false;
    if (conn_id < 0) {
        conn_id = -1;
        bool local_flag = false;
        auto localIt = m_local_devices.find(dev_id);
        if (localIt != m_local_devices.end()) {
            update_flag = true;
            auto connIt = m_lan_dev_connect_map.find(dev_id);
            if (connIt != m_lan_dev_connect_map.end()) {
                conn_id = connIt->second.id;
            }
            local_flag = true;
        }
        if (!local_flag || !wan_offline) {
            auto userIt = m_user_devices.find(dev_id);
            if (userIt != m_user_devices.end()) {
                update_flag = true;
                auto connIt = m_wan_dev_connect_map.find(dev_id);
                if (connIt != m_wan_dev_connect_map.end()) {
                    conn_id = connIt->second.id;
                }
            }
        }
        event.SetConnectionId(conn_id);
        event.SetOperator(update_flag ? DeviceListUpdateEvent::UpdateType::UpdateType_Update : DeviceListUpdateEvent::UpdateType::UpdateType_Remove);
    } else {
        event.SetConnectionId(conn_id);
        event.SetOperator(DeviceListUpdateEvent::UpdateType::UpdateType_Add);
    }
    event.SetEventObject(this);
    wxPostEvent(this, event);
}

void DeviceObjectOpr::onConnectExit(ComConnectionExitEvent &event)
{
    BOOST_LOG_TRIVIAL(info) << "DeviceData-onConnectExit: " << event.id;
    flush_logs();
    event.Skip();
    id_connect_mode mode;
    std::string     devId = find_dev_from_id(mode, event.id);
    if (devId.empty())
        return;
    DeviceObject *devObj = nullptr;
    if (mode.mode == COM_CONNECT_WAN) {
        auto it = m_user_devices.find(devId);
        if (it != m_user_devices.end()) {
            devObj = it->second;
            //if (event.ret == COM_VERIFY_LAN_DEV_FAILED) {
            //    unbind_lan_machine(devObj);
            //} else {
                //if (devObj->is_lan_mode_printer()) {
                //    devObj->set_online_state(false);
                //} else {
                    auto tmpIt = m_local_devices.find(devId);
                    if (tmpIt != m_local_devices.end()) {
                        tmpIt->second->set_device_type(DT_LOCAL);
                    }
                    removeUserDev(it->second);
                //}
            //}
        }
    } else {
        auto it = m_local_devices.find(devId);
        if (it != m_local_devices.end()) {
            devObj = it->second;
            devObj->set_connecting(false);

            if (event.ret == COM_VERIFY_LAN_DEV_FAILED) {
                auto tmpIt = m_user_devices.find(devId);
                if (tmpIt != m_user_devices.end()) {
                    tmpIt->second->set_device_type(DT_USER);
                }
                unbind_lan_machine(devObj);
            }  else {
                if (devObj->is_lan_mode_printer()) {
                    bool state = devObj->is_online();
                    devObj->set_online_state(false);
                    if (state) {
                        sendDeviceListUpdateEvent(devObj->get_dev_id(), -1);
                    }
                }
            }
        } else {
            auto it = m_scan_devices.find(devId);
            if (it != m_scan_devices.end()) {
                devObj = it->second;
                devObj->set_connecting(false);
                if (devObj->get_user_access_code().empty()) {
                    // first bind
                    if (event.ret == COM_VERIFY_LAN_DEV_FAILED) {
                        // popop input access code dialog again.
                        //ConnectPrinterDialog dlg(wxGetApp().mainframe, wxID_ANY, _L("Input access code"), true);
                        ConnectPrinterDialog dlg(true);
                        dlg.SetFocus();
                        dlg.set_device_object(devObj);
                        if (dlg.ShowModal() == wxID_OK) {
                            wxGetApp().mainframe->jump_to_monitor(devObj->get_dev_id());
                        }
                    } else if (event.ret == COM_ERROR) {
                        devObj->set_connected_ready(false); // connect finished, and failed.
                        wxGetApp().mainframe->jump_to_monitor_exit(devObj->get_dev_id());
                    } else {
                        // do nothing, this device still belongs to other device. (Including exit successfully)
                    }
                } else {
                    if (event.ret == COM_VERIFY_LAN_DEV_FAILED) {
                        // notify the device access code has changed, this device should unbind and move to other device.
                        auto tmpIt = m_user_devices.find(devId);
                        if (tmpIt != m_user_devices.end()) {
                            tmpIt->second->set_device_type(DT_USER);
                        }
                        unbind_lan_machine(devObj);
                    } else {
                        bool state = devObj->is_online();
                        devObj->set_online_state(false);
                        if (state)
                            sendDeviceListUpdateEvent(devObj->get_dev_id(), -1);
                    }
                }
            }
        } 
    }
}

void DeviceObjectOpr::onConnectReady(ComConnectionReadyEvent &event)
{
    BOOST_LOG_TRIVIAL(info) << "DeviceObjectOpr::onConnectReady: " << event.id;
    event.Skip();
    int connectId = event.id ;
    const com_dev_data_t &data      = MultiComMgr::inst()->devData(connectId);
    if (data.connectMode == COM_CONNECT_WAN) {
        std::string macSN = data.wanDevInfo.serialNumber;
        auto   it    = m_user_devices.find(macSN);
        if (it == m_user_devices.end()) {
            device_wan_info wanInfo;
            wanInfo.name = data.wanDevInfo.name;
            wanInfo.bind_dev_id = data.wanDevInfo.devId;
            wanInfo.nim_account_id = data.wanDevInfo.nimAccountId;
            wanInfo.pid = data.devDetail->pid;
            wanInfo.serialNum = data.wanDevInfo.serialNumber;

            DeviceObject *devObj = nullptr;
            it = m_old_user_devices.find(macSN);
            if (it != m_old_user_devices.end()) {
                devObj = it->second;
                //auto wan_info = new device_wan_info(wanInfo);
                devObj->set_wan_dev_info(wanInfo);
                devObj->init_wan_obj();
                m_old_user_devices.erase(it);
            } else {
                devObj = new DeviceObject(wanInfo);
            }

            //DeviceObject *devObj = new DeviceObject(wanInfo);
            devObj->set_connection_type(CONNECTTYPE_CLOUD);
            devObj->set_connecting(false);
            devObj->set_connected_ready(true);
            devObj->set_online_state(data.wanDevInfo.status != "offline");
            auto tmpIt = m_local_devices.find(macSN);
            if (tmpIt != m_local_devices.end()) {
                devObj->set_device_type(DT_BOTH);
                tmpIt->second->set_device_type(DT_BOTH);
            } else
                devObj->set_device_type(DT_USER);
            m_user_devices.emplace(make_pair(macSN, devObj));
            id_connect_mode mode;
            mode.id = connectId;
            mode.mode = COM_CONNECT_WAN;
            m_wan_dev_connect_map.emplace(make_pair(macSN, mode));
            sendDeviceListUpdateEvent(macSN, connectId);
            BOOST_LOG_TRIVIAL(info) << "Add new wan dev: " << wanInfo.name;
            flush_logs();
        }
    } else {
        std::string   serialNum = data.lanDevInfo.serialNumber;
        DeviceObject *devObj    = get_scan_device(serialNum);
        if (devObj == nullptr) {
            return;
        }

        DeviceObject *userObj = nullptr;
        auto it = m_local_devices.find(serialNum);
        if (it == m_local_devices.end()) {
            if (devObj->get_lan_dev_info() == nullptr) {
                devObj->set_lan_dev_info(data.lanDevInfo);
            }
            userObj = new DeviceObject(*devObj->get_lan_dev_info());
            userObj->set_user_access_code(devObj->get_user_access_code(true));
            id_connect_mode mode;
            mode.id   = connectId;
            mode.mode = COM_CONNECT_WAN;
            m_lan_dev_connect_map.emplace(make_pair(serialNum, mode));
            m_local_devices.emplace(make_pair(serialNum, userObj));

            AppConfig *config = GUI::wxGetApp().app_config;
            if (config) {
                config->save_bind_machine_to_config(devObj->get_dev_id(), devObj->get_dev_name(), data.devDetail->location, devObj->get_dev_pid());
            }
            sendDeviceListUpdateEvent(serialNum, connectId);
            BOOST_LOG_TRIVIAL(info) << "Add new lan dev: " << data.lanDevInfo.name;
            flush_logs();
        } else {
            sendDeviceListUpdateEvent(serialNum, connectId);
            userObj = it->second;
        }
        userObj->set_online_state(true);
        userObj->set_connecting(false);
        userObj->set_connected_ready(true);
        auto tmpIt = m_user_devices.find(serialNum);
        if (tmpIt != m_user_devices.end()) {
            userObj->set_device_type(DT_BOTH);
            tmpIt->second->set_device_type(DT_BOTH);
        } else
            userObj->set_device_type(DT_LOCAL);
    }    
}

void DeviceObjectOpr::onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event)
{
    event.Skip();
    const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
    if (data.connectMode == COM_CONNECT_WAN) {
        auto it = m_user_devices.find(data.wanDevInfo.serialNumber);
        if (it != m_user_devices.end()) {
            bool state = it->second->is_online();
            bool update = false;
            it->second->set_online_state(data.wanDevInfo.status != "offline");
            if (it->second->get_dev_name() != data.wanDevInfo.name) {
                it->second->set_dev_name(data.wanDevInfo.name);
                BOOST_LOG_TRIVIAL(info) << it->second->get_dev_name() << "  update dev name";
                flush_logs();
                update = true;
            }
            if (state != it->second->is_online()) {      
                BOOST_LOG_TRIVIAL(info) << it->second->get_dev_name()<<"  update online state";
                flush_logs();
                update = true;
            }
            if (update) {
                sendDeviceListUpdateEvent(data.wanDevInfo.serialNumber, it->second->is_online() ? event.id : -1, true);
            }
        }
    }
}

}
}
