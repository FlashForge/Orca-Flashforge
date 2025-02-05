#ifndef slic3r_GUI_MultiComEvent_hpp_
#define slic3r_GUI_MultiComEvent_hpp_

#include <wx/event.h>
#include <map>
#include "MultiComDef.hpp"

namespace Slic3r { namespace GUI {

struct ComConnectionEvent : public wxCommandEvent
{
    ComConnectionEvent(wxEventType type, com_id_t _id, int _commandId)
        : wxCommandEvent(type)
        , id(_id)
        , commandId(_commandId)
    {
    }
    com_id_t id;
    int commandId;
};

struct ComConnectionReadyEvent : public ComConnectionEvent
{
    ComConnectionReadyEvent(wxEventType type, com_id_t _id, fnet_dev_product_t *_devProduct, fnet_dev_detail_t *_devDetail)
        : ComConnectionEvent(type, _id, ComInvalidCommandId)
        , devProduct(_devProduct)
        , devDetail(_devDetail)
    {
    }
    ComConnectionReadyEvent *Clone() const
    {
        return new ComConnectionReadyEvent(GetEventType(), id, devProduct, devDetail);
    }
    fnet_dev_product_t *devProduct;
    fnet_dev_detail_t *devDetail;
};

struct ComConnectionExitEvent : public ComConnectionEvent
{
    ComConnectionExitEvent(wxEventType type, com_id_t _id, ComErrno _ret)
        : ComConnectionEvent(type, _id, ComInvalidCommandId)
        , ret(_ret)
    {
    }
    ComConnectionExitEvent *Clone() const
    {
        return new ComConnectionExitEvent(GetEventType(), id, ret);
    }
    ComErrno ret;
};

struct ComWanDevInfoUpdateEvent : public ComConnectionEvent
{
    ComWanDevInfoUpdateEvent(wxEventType type, com_id_t _id)
        : ComConnectionEvent(type, _id, ComInvalidCommandId)
    {
    }
    ComWanDevInfoUpdateEvent *Clone() const
    {
        return new ComWanDevInfoUpdateEvent(GetEventType(), id);
    }
};

struct ComDevDetailUpdateEvent : public ComConnectionEvent
{
    ComDevDetailUpdateEvent(wxEventType type, com_id_t _id, int _commandId, fnet_dev_detail_t *_devDetail)
        : ComConnectionEvent(type, _id, _commandId)
        , devDetail(_devDetail)
    {
    }
    ComDevDetailUpdateEvent *Clone() const
    {
        return new ComDevDetailUpdateEvent(GetEventType(), id, commandId, devDetail);
    }
    fnet_dev_detail_t *devDetail;
};

struct ComGetDevGcodeListEvent : public ComConnectionEvent 
{
    ComGetDevGcodeListEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret,
        fnet_lan_gcode_list_t *_lanGcodeList, fnet_wan_gcode_list_t *_wanGcodeList)
        : ComConnectionEvent(type, _id, _commandId)
        , ret(_ret)
        , lanGcodeList(_lanGcodeList)
        , wanGcodeList(_wanGcodeList)
    {
    }
    ComGetDevGcodeListEvent *Clone() const
    {
        return new ComGetDevGcodeListEvent(GetEventType(), id, commandId, ret, lanGcodeList, wanGcodeList);
    }
    ComErrno ret;
    fnet_lan_gcode_list_t *lanGcodeList;
    fnet_wan_gcode_list_t *wanGcodeList;
};

struct ComGetGcodeThumbEvent : public ComConnectionEvent 
{
    ComGetGcodeThumbEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret, std::vector<char> &_thumbData)
        : ComConnectionEvent(type, _id, _commandId)
        , ret(_ret)
        , thumbData(std::move(_thumbData))
    {
    }
    ComGetGcodeThumbEvent *MoveClone()
    {
        return new ComGetGcodeThumbEvent(GetEventType(), id, commandId, ret, thumbData);
    }
    ComErrno ret;
    std::vector<char> thumbData;
};

struct ComStartJobEvent : public ComConnectionEvent
{
    ComStartJobEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret)
        : ComConnectionEvent(type, _id, _commandId)
        , ret(_ret)
    {
    }
    ComStartJobEvent *Clone() const
    {
        return new ComStartJobEvent(GetEventType(), id, commandId, ret);
    }
    ComErrno ret;
};

struct ComSendGcodeProgressEvent : public ComConnectionEvent
{
    ComSendGcodeProgressEvent(wxEventType type, com_id_t _id, int _commandId, double _now, double _total) 
        : ComConnectionEvent(type, _id, _commandId)
        , now(_now)
        , total(_total)
    {
    }
    ComSendGcodeProgressEvent(wxEventType type, double _now, double _total) 
        : ComConnectionEvent(type, ComInvalidId, ComInvalidCommandId)
        , now(_now)
        , total(_total)
    {
    }
    ComSendGcodeProgressEvent *Clone() const
    {
        return new ComSendGcodeProgressEvent(GetEventType(), id, commandId, now, total);
    }
    double now;
    double total;
};

struct ComSendGcodeFinishEvent : public ComConnectionEvent
{
    ComSendGcodeFinishEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret)
        : ComConnectionEvent(type, _id, _commandId)
        , ret(_ret)
    {
    }
    ComSendGcodeFinishEvent(wxEventType type, const fnet_clound_job_error_t *errors, int errorCnt, ComErrno _ret)
        : ComConnectionEvent(type, ComInvalidId, ComInvalidCommandId)
        , ret(_ret)
    {
        for (int i = 0; i < errorCnt; ++i) {
            errorMap.emplace(errors[i].devId, errors[i].type);
        }
    }
    ComSendGcodeFinishEvent *Clone() const
    {
        auto event = new ComSendGcodeFinishEvent(GetEventType(), id, commandId, ret);
        event->errorMap = errorMap;
        return event;
    }
    std::map<std::string, fnet_clound_job_error_type_t> errorMap;
    ComErrno ret;
};

struct ComWanDevMaintainEvent : public wxCommandEvent
{
    ComWanDevMaintainEvent(wxEventType type, bool _login, bool _online, ComErrno _ret)
        : wxCommandEvent(type)
        , login(_login)
        , online(_online)
        , ret(_ret)
    {
    }
    ComWanDevMaintainEvent *Clone() const
    {
        return new ComWanDevMaintainEvent(GetEventType(), login, online, ret);
    }
    bool login;
    bool online;
    ComErrno ret;
};

struct ComGetUserProfileEvent : public wxCommandEvent
{
    ComGetUserProfileEvent(wxEventType type, const com_user_profile_t &_userProfile, ComErrno _ret)
        : wxCommandEvent(type)
        , userProfile(_userProfile)
        , ret(_ret)
    {
    }
    ComGetUserProfileEvent *Clone() const
    {
        return new ComGetUserProfileEvent(GetEventType(), userProfile, ret);
    }
    com_user_profile_t userProfile;
    ComErrno ret;
};

struct ComRefreshTokenEvent : public wxCommandEvent
{
    ComRefreshTokenEvent(wxEventType type, const com_token_data_t &_tokenData, ComErrno _ret)
        : wxCommandEvent(type)
        , tokenData(_tokenData)
        , ret(_ret)
    {
    }
    ComRefreshTokenEvent *Clone() const
    {
        return new ComRefreshTokenEvent(GetEventType(), tokenData, ret);
    }
    com_token_data_t tokenData;
    ComErrno ret;
};

wxDECLARE_EVENT(COM_CONNECTION_READY_EVENT, ComConnectionReadyEvent);
wxDECLARE_EVENT(COM_CONNECTION_EXIT_EVENT, ComConnectionExitEvent);
wxDECLARE_EVENT(COM_WAN_DEV_INFO_UPDATE_EVENT, ComWanDevInfoUpdateEvent);
wxDECLARE_EVENT(COM_DEV_DETAIL_UPDATE_EVENT, ComDevDetailUpdateEvent);
wxDECLARE_EVENT(COM_GET_DEV_GCODE_LIST_EVENT, ComGetDevGcodeListEvent);
wxDECLARE_EVENT(COM_GET_GCODE_THUMB_EVENT, ComGetGcodeThumbEvent);
wxDECLARE_EVENT(COM_START_JOB_EVENT, ComStartJobEvent);
wxDECLARE_EVENT(COM_SEND_GCODE_PROGRESS_EVENT, ComSendGcodeProgressEvent);
wxDECLARE_EVENT(COM_SEND_GCODE_FINISH_EVENT, ComSendGcodeFinishEvent);
wxDECLARE_EVENT(COM_WAN_DEV_MAINTAIN_EVENT, ComWanDevMaintainEvent);
wxDECLARE_EVENT(COM_GET_USER_PROFILE_EVENT, ComGetUserProfileEvent);
wxDECLARE_EVENT(COM_REFRESH_TOKEN_EVENT, ComRefreshTokenEvent);

}} // namespace Slic3r::GUI

#endif
