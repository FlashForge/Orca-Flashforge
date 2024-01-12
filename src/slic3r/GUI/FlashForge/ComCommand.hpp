#ifndef slic3r_GUI_ComCommand_hpp_
#define slic3r_GUI_ComCommand_hpp_

#include <atomic>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

class ComCommand
{
public:
    ComCommand()
        : m_commandId(s_commandNum++)
    {
    }
    virtual ~ComCommand()
    {
    }
    int commandId() const
    {
        return m_commandId;
    }
    virtual bool isDup(const ComCommand *that)
    {
        return typeid(*this) == typeid(*that);
    }
    virtual ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode) = 0;

    virtual ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &accessToken,
        const std::string &deviceId) = 0;

protected:
    int m_commandId;
    static int s_commandNum;
};

class ComGetDevDetail : public ComCommand
{
public:
    ComGetDevDetail()
        : m_devDetail(nullptr)
    {
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->getLanDevDetail(
            ip.c_str(), port, serialNumber.c_str(), checkCode.c_str(), &m_devDetail, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &accessToken,
        const std::string &deviceId)
    {
        int ret = networkIntfc->getWanDevDetail(
            accessToken.c_str(), deviceId.c_str(), &m_devDetail, ComTimeoutWan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    fnet_dev_detail_t *devDetail()
    {
        return m_devDetail;
    }
 
private:
    fnet_dev_detail_t *m_devDetail;
};

class ComSendGcode : public ComCommand
{
public:
    ComSendGcode(const std::string &gcodeFileName, const std::string &thumbFileName,
        bool printNow, bool levelingBeforePrint)
        : m_progress(0)
        , m_callbackRet(0)
        , m_comId(ComInvalidId)
        , m_evtHandler(nullptr)
        , m_gcodeFileName(gcodeFileName)
        , m_thumbFileName(thumbFileName)
    {
        m_sendGcodeData.gcodeFileName = m_gcodeFileName.c_str();
        m_sendGcodeData.thumbFileName = m_thumbFileName.c_str();
        m_sendGcodeData.printNow = printNow;
        m_sendGcodeData.levelingBeforePrint = levelingBeforePrint;
        m_sendGcodeData.callback = callback;
        m_sendGcodeData.callbackData = this;
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->lanDevSendGcode(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_sendGcodeData, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &accessToken,
        const std::string &deviceId)
    {
        int ret = networkIntfc->wanDevSendGcode(
            accessToken.c_str(), deviceId.c_str(), &m_sendGcodeData, ComTimeoutWan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void abort()
    {
        m_callbackRet = 1;
    }
    void setConectionData(com_id_t comId, wxEvtHandler *evtHandler)
    {
        m_comId = comId;
        m_evtHandler = evtHandler;
    }

private:
    static int callback(long long now, long long total, void *callbackData)
    {
        ComSendGcode *inst = (ComSendGcode *) callbackData;
        if (total != 0) {
            double progress = (double)now / total;
            if (progress - inst->m_progress > 0.025 && inst->m_evtHandler != nullptr) {
                inst->m_evtHandler->QueueEvent(new ComSendGcodeProgressEvent(
                    COM_SEND_GCODE_PROGRESS_EVENT, inst->m_comId, inst->m_commandId, now, total));
                inst->m_progress = progress;
            }
        }
        return inst->m_callbackRet;
    }

private:
    double                  m_progress;
    std::atomic<int>        m_callbackRet;
    com_id_t                m_comId;
    wxEvtHandler           *m_evtHandler;
    std::string             m_gcodeFileName;
    std::string             m_thumbFileName;
    fnet_send_gcode_data_t  m_sendGcodeData;
};

}} // namespace Slic3r::GUI

#endif
