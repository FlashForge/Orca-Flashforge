#ifndef slic3r_GUI_ComCommand_hpp_
#define slic3r_GUI_ComCommand_hpp_

#include <atomic>
#include <wx/event.h>
#include "ComWanAsyncConn.hpp"
#include "FlashNetworkIntfc.h"
#include "FreeInDestructor.h"
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

    virtual ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId) = 0;

protected:
    int m_commandId;
    static int s_commandNum;
};

class ComGetDevProduct : public ComCommand
{
public:
    ComGetDevProduct()
        : m_devProduct(nullptr)
    {
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->getLanDevProduct(
            ip.c_str(), port, serialNumber.c_str(), checkCode.c_str(), &m_devProduct, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        return COM_ERROR;
    }
    fnet_dev_product_t *devProduct()
    {
        return m_devProduct;
    }
 
private:
    fnet_dev_product_t *m_devProduct;
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
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        return COM_ERROR;
    }
    fnet_dev_detail_t *devDetail()
    {
        return m_devDetail;
    }
 
private:
    fnet_dev_detail_t *m_devDetail;
};

class ComGetDevProductDetail : public ComCommand
{
public:
    ComGetDevProductDetail()
        : m_devProduct(nullptr)
        , m_devDetail(nullptr)
    {
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        return COM_ERROR;
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        int ret = networkIntfc->getWanDevProductDetail(uid.c_str(), accessToken.c_str(),
            deviceId.c_str(), &m_devProduct, &m_devDetail, ComTimeoutWan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    fnet_dev_product_t *devProduct()
    {
        return m_devProduct;
    }
    fnet_dev_detail_t *devDetail()
    {
        return m_devDetail;
    }
 
private:
    fnet_dev_product_t *m_devProduct;
    fnet_dev_detail_t *m_devDetail;
};

class ComGetDevGcodeList : public ComCommand
{
public:
    ComGetDevGcodeList()
        : m_lanGcodeList(nullptr)
        , m_wanGcodeList(nullptr)
    {
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->getLanDevGcodeList(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_lanGcodeList, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        int ret = networkIntfc->getWanDevGcodeList(uid.c_str(), accessToken.c_str(),
            deviceId.c_str(), &m_wanGcodeList, ComTimeoutWan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    fnet_lan_gcode_list_t *lanGcodeList()
    {
        return m_lanGcodeList;
    }
    fnet_wan_gcode_list_t *wanGcodeList()
    {
        return m_wanGcodeList;
    }

private:
    fnet_lan_gcode_list_t *m_lanGcodeList;
    fnet_wan_gcode_list_t *m_wanGcodeList;
};

class ComGetGcodeThumb : public ComCommand
{
public:
    ComGetGcodeThumb(const std::string &fileNameOrThumbUrl)
        : m_fileNameOrThumbUrl(fileNameOrThumbUrl)
    {
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        fnet_file_data_t *fileData;
        int fnetRet = networkIntfc->getLanDevGcodeThumb(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), m_fileNameOrThumbUrl.c_str(), &fileData, 15000);
        if (fnetRet != FNET_OK) {
            return MultiComUtils::fnetRet2ComErrno(fnetRet);
        }
        fnet::FreeInDestructor freeFileData(fileData, networkIntfc->freeFileData);
        m_thumbData.assign(fileData->data, fileData->data + fileData->size);
        return COM_OK;
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        return MultiComUtils::downloadFile(m_fileNameOrThumbUrl, m_thumbData, 15000);
    }
    std::vector<char> &thumbData()
    {
        return m_thumbData;
    }

private:
    std::string m_fileNameOrThumbUrl;
    std::vector<char> m_thumbData;
};

class ComStartJob : public ComCommand
{
public:
    ComStartJob(const std::string &fileName, bool levelingBeforePrint)
        : m_fileName(fileName)
        , m_levelingBeforePrint(levelingBeforePrint)
    {
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->lanDevStartJob(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), m_fileName.c_str(), m_levelingBeforePrint, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        int ret = networkIntfc->wanDevStartJob(uid.c_str(), accessToken.c_str(),
            deviceId.c_str(), m_fileName.c_str(), m_levelingBeforePrint, ComTimeoutWan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }

private:
    std::string m_fileName;
    int m_levelingBeforePrint;
};

class ComSendGcode : public ComCommand
{
public:
    ComSendGcode(const std::string &gcodeFilePath, const std::string &thumbFilePath,
        const std::string &gcodeDstName, bool printNow, bool levelingBeforePrint)
        : m_progress(0)
        , m_callbackRet(0)
        , m_comId(ComInvalidId)
        , m_evtHandler(nullptr)
        , m_gcodeFilePath(gcodeFilePath)
        , m_thumbFilePath(thumbFilePath)
        , m_gcodeDstName(gcodeDstName)
    {
        m_sendGcodeData.gcodeFilePath = m_gcodeFilePath.c_str();
        m_sendGcodeData.thumbFilePath = m_thumbFilePath.c_str();
        m_sendGcodeData.gcodeDstName = m_gcodeDstName.c_str();
        m_sendGcodeData.printNow = printNow;
        m_sendGcodeData.levelingBeforePrint = levelingBeforePrint;
        m_sendGcodeData.callback = callback;
        m_sendGcodeData.callbackData = this;
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->lanDevSendGcode(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_sendGcodeData, 15000);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        int ret = networkIntfc->wanDevSendGcode(
            uid.c_str(), accessToken.c_str(), deviceId.c_str(), &m_sendGcodeData, 15000);
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
    std::string             m_gcodeFilePath;
    std::string             m_thumbFilePath;
    std::string             m_gcodeDstName;
    fnet_send_gcode_data_t  m_sendGcodeData;
};

class ComWanAsyncCommand : public ComCommand
{
public:
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &uid,
        const std::string &accessToken, const std::string &deviceId)
    {
        return COM_ERROR;
    }
    virtual void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId) = 0;
};

class ComTempCtrl : public ComWanAsyncCommand
{
public:
    ComTempCtrl(double platformTemp, double rightTemp, double leftTemp, double chamberTemp)
    {
        m_tempCtrl.platformTemp = platformTemp;
        m_tempCtrl.rightTemp = rightTemp;
        m_tempCtrl.leftTemp = leftTemp;
        m_tempCtrl.chamberTemp = chamberTemp;
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->ctrlLanDevTemp(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_tempCtrl, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postTempCtrl(devId, m_tempCtrl);
    }

private:
    fnet_temp_ctrl_t m_tempCtrl;
};

class ComLightCtrl : public ComWanAsyncCommand
{
public:
    ComLightCtrl(const std::string &lightStatus)
        : m_lightStatus(lightStatus)
    {
        m_lightCtrl.lightStatus = m_lightStatus.c_str();
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->ctrlLanDevLight(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_lightCtrl, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postLightCtrl(devId, m_lightCtrl);
    }

private:
    std::string m_lightStatus;
    fnet_light_ctrl_t m_lightCtrl;
};

class ComAirFilterCtrl : public ComWanAsyncCommand
{
public:
    ComAirFilterCtrl(const std::string &internalFanStatus, const std::string &externalFanStatus)
        : m_internalFanStatus(internalFanStatus)
        , m_externalFanStatus(externalFanStatus)
    {
        m_airFilterCtrl.internalFanStatus = m_internalFanStatus.c_str();
        m_airFilterCtrl.externalFanStatus = m_externalFanStatus.c_str();
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->ctrlLanDevAirFilter(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_airFilterCtrl, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postAirFilterCtrl(devId, m_airFilterCtrl);
    }

private:
    std::string m_internalFanStatus;
    std::string m_externalFanStatus;
    fnet_air_filter_ctrl_t m_airFilterCtrl;
};

class ComClearFanCtrl : public ComWanAsyncCommand
{
public:
    ComClearFanCtrl(const std::string &clearFanStatus)
        : m_clearFanStatus(clearFanStatus)
    {
        m_clearFanCtrl.clearFanStatus = m_clearFanStatus.c_str();
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->ctrlLanDevClearFan(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_clearFanCtrl, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postClearFanCtrl(devId, m_clearFanCtrl);
    }

private:
    std::string m_clearFanStatus;
    fnet_clear_fan_ctrl_t m_clearFanCtrl;
};

class ComPrintCtrl : public ComWanAsyncCommand
{
public:
    ComPrintCtrl(double zAxisCompensation, double printSpeedAdjust, double coolingFanSpeed,
        double coolingFanLeftSpeed, double chamberFanSpeed)
    {
        m_printCtrl.zAxisCompensation = zAxisCompensation;
        m_printCtrl.printSpeedAdjust = printSpeedAdjust;
        m_printCtrl.coolingFanSpeed = coolingFanSpeed;
        m_printCtrl.coolingFanLeftSpeed = coolingFanLeftSpeed;
        m_printCtrl.chamberFanSpeed = chamberFanSpeed;
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->ctrlLanDevPrint(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_printCtrl, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postPrintCtrl(devId, m_printCtrl);
    }

private:
    fnet_print_ctrl_t m_printCtrl;
};

class ComJobCtrl : public ComWanAsyncCommand
{
public:
    ComJobCtrl(const std::string &jobId, const std::string &action)
        : m_jobId(jobId)
        , m_action(action)
    {
        m_jobCtrl.jobId = m_jobId.c_str();
        m_jobCtrl.action = m_action.c_str();
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        int ret = networkIntfc->ctrlLanDevJob(ip.c_str(), port, serialNumber.c_str(),
            checkCode.c_str(), &m_jobCtrl, ComTimeoutLan);
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postJobCtrl(devId, m_jobCtrl);
    }

private:
    std::string m_jobId;
    std::string m_action;
    fnet_job_ctrl_t m_jobCtrl;
};

class ComCameraStreamCtrl : public ComWanAsyncCommand
{
public:
    ComCameraStreamCtrl(const std::string &action)
        : m_action(action)
    {
        m_cameraStreamCtrl.action = m_action.c_str();
    }
    ComErrno exec(fnet::FlashNetworkIntfc *networkIntfc, const std::string &ip,
        unsigned int port, const std::string &serialNumber, const std::string &checkCode)
    {
        return COM_OK;
    }
    void asyncExec(ComWanAsyncConn *wanAsyncConn, const std::string &devId)
    {
        wanAsyncConn->postCameraStreamCtrl(devId, m_cameraStreamCtrl);
    }

private:
    std::string m_action;
    fnet_camera_stream_ctrl_t m_cameraStreamCtrl;
};

}} // namespace Slic3r::GUI

#endif
