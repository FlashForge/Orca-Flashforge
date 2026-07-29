#ifndef slic3r_GUI_MultiComEvent_hpp_
#define slic3r_GUI_MultiComEvent_hpp_

#include <cstdint>
#include <map>
#include <wx/event.h>
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

struct ComCloudSliceUpdateEvent : public ComConnectionEvent
{
    ComCloudSliceUpdateEvent(wxEventType type, com_id_t _id, int _commandId, fnet_slice_state_t* _state)
        : ComConnectionEvent(type, _id, _commandId), state(_state)
    {}
    ComCloudSliceUpdateEvent* Clone() const { return new ComCloudSliceUpdateEvent(GetEventType(), id, commandId, state); }
    fnet_slice_state_t*       state;
};

struct ComJobInfoUpdateEvent : public ComConnectionEvent
{
    ComJobInfoUpdateEvent(wxEventType type, com_id_t _id, int _commandId, fnet_job_info_t* _info)
        : ComConnectionEvent(type, _id, _commandId), info(_info)
    {}
    ComJobInfoUpdateEvent* Clone() const { return new ComJobInfoUpdateEvent(GetEventType(), id, commandId, info); }
    fnet_job_info_t*       info;
};

struct ComGetDevGcodeListEvent : public ComConnectionEvent 
{
    ComGetDevGcodeListEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret,
        const com_gcode_list_t &_lanGcodeList, const com_gcode_list_t &_wanGcodeList)
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
    com_gcode_list_t lanGcodeList;
    com_gcode_list_t wanGcodeList;
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

struct ComGetTimeLapseVideoListEvent : public ComConnectionEvent
{
    ComGetTimeLapseVideoListEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret,
        const com_time_lapse_video_list_t &_wanTimeLapseVideoList)
        : ComConnectionEvent(type, _id, _commandId)
        , ret(_ret)
        , wanTimeLapseVideoList(_wanTimeLapseVideoList)
    {
    }
    ComGetTimeLapseVideoListEvent *Clone() const
    {
        return new ComGetTimeLapseVideoListEvent(GetEventType(), id, commandId, ret, wanTimeLapseVideoList);
    }
    ComErrno ret;
    com_time_lapse_video_list_t wanTimeLapseVideoList;
};

struct ComDeleteTimeLapseVideoEvent : public ComConnectionEvent
{
    ComDeleteTimeLapseVideoEvent(wxEventType type, com_id_t _id, int _commandId, ComErrno _ret)
        : ComConnectionEvent(type, _id, _commandId)
        , ret(_ret)
    {
    }
    ComDeleteTimeLapseVideoEvent *Clone() const
    {
        return new ComDeleteTimeLapseVideoEvent(GetEventType(), id, commandId, ret);
    }
    ComErrno ret;
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
    ComSendGcodeFinishEvent(wxEventType type, const std::map<std::string, ComCloundJobErrno> &_errorMap,
        ComErrno _ret)
        : ComConnectionEvent(type, ComInvalidId, ComInvalidCommandId)
        , errorMap(_errorMap)
        , ret(_ret)
    {
    }
    ComSendGcodeFinishEvent *Clone() const
    {
        auto event = new ComSendGcodeFinishEvent(GetEventType(), id, commandId, ret);
        event->errorMap = errorMap;
        return event;
    }
    std::map<std::string, ComCloundJobErrno> errorMap;
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

struct ComBusRequestEvent : public wxCommandEvent
{
    ComBusRequestEvent(wxEventType type, int64_t _requestId, const std::string &_message, ComErrno _ret)
        : wxCommandEvent(type)
        , requestId(_requestId)
        , message(_message)
        , ret(_ret)
    {
    }
    ComBusRequestEvent *Clone() const
    {
        return new ComBusRequestEvent(GetEventType(), requestId, message, ret);
    }
    int64_t requestId;
    std::string message;
    ComErrno ret;
};

struct ComBusGetRequestEvent : public ComBusRequestEvent
{
    ComBusGetRequestEvent(wxEventType type, int64_t _requestId, const std::string &_message,
        const std::string &_responseData, ComErrno _ret)
        : ComBusRequestEvent(type, _requestId, _message, _ret)
        , responseData(_responseData)
    {
    }
    ComBusGetRequestEvent *Clone() const
    {
        return new ComBusGetRequestEvent(GetEventType(), requestId, message, responseData, ret);
    }
    std::string responseData;
};

struct ComBusPostRequestEvent : public ComBusRequestEvent
{
    ComBusPostRequestEvent(wxEventType type, int64_t _requestId, const std::string &_message, int _code,
        ComErrno _ret)
        : ComBusRequestEvent(type, _requestId, _message, _ret)
        , code(_code)
    {
    }
    ComBusPostRequestEvent *Clone() const
    {
        return new ComBusPostRequestEvent(GetEventType(), requestId, message, code, ret);
    }
    int code;
};

struct ComConnSysNotifyEvent : public wxCommandEvent
{
    ComConnSysNotifyEvent(wxEventType type, const std::string &_payload)
        : wxCommandEvent(type)
        , payload(_payload)
    {
    }
    ComConnSysNotifyEvent *Clone() const
    {
        return new ComConnSysNotifyEvent(GetEventType(), payload);
    }
    std::string payload;
};

wxDECLARE_EVENT(COM_CONNECTION_READY_EVENT, ComConnectionReadyEvent);
wxDECLARE_EVENT(COM_CONNECTION_EXIT_EVENT, ComConnectionExitEvent);
wxDECLARE_EVENT(COM_WAN_DEV_INFO_UPDATE_EVENT, ComWanDevInfoUpdateEvent);
wxDECLARE_EVENT(COM_DEV_DETAIL_UPDATE_EVENT, ComDevDetailUpdateEvent);
wxDECLARE_EVENT(COM_GET_DEV_GCODE_LIST_EVENT, ComGetDevGcodeListEvent);
wxDECLARE_EVENT(COM_GET_GCODE_THUMB_EVENT, ComGetGcodeThumbEvent);
wxDECLARE_EVENT(COM_GET_TIME_LAPSE_VIDEO_LIST_EVENT, ComGetTimeLapseVideoListEvent);
wxDECLARE_EVENT(COM_DELETE_TIME_LAPSE_VIDEO_EVENT, ComDeleteTimeLapseVideoEvent);
wxDECLARE_EVENT(COM_START_JOB_EVENT, ComStartJobEvent);
wxDECLARE_EVENT(COM_SEND_GCODE_PROGRESS_EVENT, ComSendGcodeProgressEvent);
wxDECLARE_EVENT(COM_SEND_GCODE_FINISH_EVENT, ComSendGcodeFinishEvent);
wxDECLARE_EVENT(COM_WAN_DEV_MAINTAIN_EVENT, ComWanDevMaintainEvent);
wxDECLARE_EVENT(COM_GET_USER_PROFILE_EVENT, ComGetUserProfileEvent);
wxDECLARE_EVENT(COM_REFRESH_TOKEN_EVENT, ComRefreshTokenEvent);
wxDECLARE_EVENT(COM_ADD_PRINT_LIST_MODEL_EVENT, ComBusRequestEvent);
wxDECLARE_EVENT(COM_REMOVE_PRINT_LIST_MODEL_EVENT, ComBusRequestEvent);
wxDECLARE_EVENT(COM_LIKE_MODEL_EVENT, ComBusRequestEvent);
wxDECLARE_EVENT(COM_REPORT_MODEL_EVENT, ComBusRequestEvent);
wxDECLARE_EVENT(COM_BUS_GET_REQUEST_EVENT, ComBusGetRequestEvent);
wxDECLARE_EVENT(COM_BUS_POST_REQUEST_EVENT, ComBusPostRequestEvent);
wxDECLARE_EVENT(COM_CONN_SYS_NOTIFY_EVENT, ComConnSysNotifyEvent);
wxDECLARE_EVENT(COM_CONN_UPDATE_NOTIFY_EVENT, ComConnSysNotifyEvent);
wxDECLARE_EVENT(COM_CONN_CLOUD_SLICE_EVENT, ComCloudSliceUpdateEvent);
wxDECLARE_EVENT(COM_CONN_JOB_INFO_EVENT, ComJobInfoUpdateEvent);

}} // namespace Slic3r::GUI

#endif
