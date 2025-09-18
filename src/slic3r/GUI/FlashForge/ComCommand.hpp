#ifndef slic3r_GUI_ComCommand_hpp_
#define slic3r_GUI_ComCommand_hpp_

#include <atomic>
#include <wx/event.h>
#include "ComWanNimConn.hpp"
#include "FlashNetworkIntfc.h"
#include "FreeInDestructor.h"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

struct com_command_exec_data_t {
    ComConnectMode connectMode;
    fnet::FlashNetworkIntfc *networkIntfc;
    const char *ip;
    unsigned int port;
    const char *serialNumber;
    const char *checkCode;
    const char *uid;
    const char *accessToken;
    const char *deviceId;
    const char *nimAccountId;
};

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
    virtual ComErrno exec(const com_command_exec_data_t &data) = 0;

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
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = data.networkIntfc->getLanDevProduct(
                data.ip, data.port, data.serialNumber, data.checkCode, &m_devProduct, ComTimeoutLanA);
        } else {
            ret = FNET_ERROR;
        }
        return MultiComUtils::fnetRet2ComErrno(ret);
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
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = data.networkIntfc->getLanDevDetail(
                data.ip, data.port, data.serialNumber, data.checkCode, &m_devDetail, ComTimeoutLanA);
        } else {
            ret = FNET_ERROR;
        }
        return MultiComUtils::fnetRet2ComErrno(ret);
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
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = FNET_ERROR;
        } else {
            ret = data.networkIntfc->getWanDevProductDetail(data.uid, data.accessToken,
                data.deviceId, &m_devProduct, &m_devDetail, ComTimeoutWanB);
        }
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
    {
        m_lanGcodeList.gcodeCnt = 0;
        m_lanGcodeList.gcodeDatas = nullptr;
        m_wanGcodeList.gcodeCnt = 0;
        m_wanGcodeList.gcodeDatas = nullptr;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = data.networkIntfc->getLanDevGcodeList(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_lanGcodeList.gcodeDatas, &m_lanGcodeList.gcodeCnt, ComTimeoutLanA);
        } else {
            ret = data.networkIntfc->getWanDevGcodeList(data.uid, data.accessToken,
                data.deviceId, &m_wanGcodeList.gcodeDatas, &m_wanGcodeList.gcodeCnt, ComTimeoutWanA);
        }
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    const com_gcode_list_t &lanGcodeList()
    {
        return m_lanGcodeList;
    }
    const com_gcode_list_t &wanGcodeList()
    {
        return m_wanGcodeList;
    }

private:
    com_gcode_list_t m_lanGcodeList;
    com_gcode_list_t m_wanGcodeList;
};

class ComGetGcodeThumb : public ComCommand
{
public:
    ComGetGcodeThumb(const std::string &fileNameOrThumbUrl)
        : m_fileNameOrThumbUrl(fileNameOrThumbUrl)
    {
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            fnet_file_data_t *fileData;
            int ret = data.networkIntfc->getLanDevGcodeThumb(data.ip, data.port, data.serialNumber,
                data.checkCode, m_fileNameOrThumbUrl.c_str(), &fileData, ComTimeoutLanB);
            if (ret != FNET_OK) {
                return MultiComUtils::fnetRet2ComErrno(ret);
            }
            fnet::FreeInDestructor freeFileData(fileData, data.networkIntfc->freeFileData);
            m_thumbData.assign(fileData->data, fileData->data + fileData->size);
            return COM_OK;
        } else {
            return MultiComUtils::downloadFileMem(
                m_fileNameOrThumbUrl, m_thumbData, nullptr, nullptr, ComTimeoutWanB, ComTimeoutWanB);
        }
    }
    std::vector<char> &thumbData()
    {
        return m_thumbData;
    }

private:
    std::string m_fileNameOrThumbUrl;
    std::vector<char> m_thumbData;
};

class ComGetTimeLapseVideoList : public ComCommand
{
public:
    ComGetTimeLapseVideoList()
    {
        m_wanTimeLapseVideoList.videoCnt = 0;
        m_wanTimeLapseVideoList.videoDatas = nullptr;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = FNET_ERROR;
        } else {
            ret = data.networkIntfc->getWanDevTimeLapseVideoList(
                data.uid, data.accessToken, data.deviceId, 15, &m_wanTimeLapseVideoList.videoDatas,
                &m_wanTimeLapseVideoList.videoCnt, ComTimeoutWanA);
        }
        return MultiComUtils::fnetRet2ComErrno(ret);
    }
    const com_time_lapse_video_list_t &wanTimeLapseVideoList()
    {
        return m_wanTimeLapseVideoList;
    }

private:
    com_time_lapse_video_list_t m_wanTimeLapseVideoList;
};

class ComDeleteTimeLapseVideo : public ComCommand
{
public:
    ComDeleteTimeLapseVideo(const std::vector<std::string> &jobIds)
        : m_jobIds(jobIds)
    {
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = FNET_ERROR;
        } else {
            std::vector<const char *> jobIdPtrs(m_jobIds.size());
            for (size_t i = 0; i < m_jobIds.size(); ++i) {
                jobIdPtrs[i] = m_jobIds[i].c_str();
            }
            ret = data.networkIntfc->deleteTimeLapseVideo(
                data.uid, data.accessToken, jobIdPtrs.data(), jobIdPtrs.size(), ComTimeoutWanA);
        }
        return MultiComUtils::fnetRet2ComErrno(ret);
    }

private:
    std::vector<std::string> m_jobIds;
};

class ComStartJob : public ComCommand
{
public:
    ComStartJob(const com_local_job_data_t &comJobData)
        : m_comJobData(comJobData)
    {
        m_materialMappings = MultiComUtils::comMaterialMappings2Fnet(m_comJobData.materialMappings);
        m_jobData.jobId = nullptr;
        m_jobData.thumbUrl = nullptr;
        m_jobData.fileName = m_comJobData.fileName.c_str();
        m_jobData.printNow = m_comJobData.printNow;
        m_jobData.levelingBeforePrint = m_comJobData.levelingBeforePrint;
        m_jobData.flowCalibration = m_comJobData.flowCalibration;
        m_jobData.firstLayerInspection = m_comJobData.firstLayerInspection;
        m_jobData.timeLapseVideo = m_comJobData.timeLapseVideo;
        m_jobData.useMatlStation = m_comJobData.useMatlStation;
        m_jobData.gcodeToolCnt = (int)m_comJobData.materialMappings.size();
        m_jobData.materialMappings = m_materialMappings.data();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->lanDevStartJob(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_jobData, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            fnet_add_job_result_t *result = nullptr;
            int ret = data.networkIntfc->wanDevAddJob(data.uid, data.accessToken,
                data.deviceId, &m_jobData, &result, ComTimeoutWanA);
            if (ret != FNET_OK) {
                return MultiComUtils::fnetRet2ComErrno(ret);
            }
            fnet::FreeInDestructor freeResult(result, data.networkIntfc->freeAddJobResult);
            m_jobData.jobId = result->jobId;
            m_jobData.thumbUrl = result->thumbUrl;
            return ComWanNimConn::inst()->sendStartJob(data.nimAccountId, m_jobData);
        }
    }

private:
    fnet_local_job_data_t m_jobData;
    com_local_job_data_t  m_comJobData;
    std::vector<fnet_material_mapping_t> m_materialMappings;
};

class ComSendGcode : public ComCommand
{
public:
    ComSendGcode(const com_send_gcode_data_t &comSendGcodeData)
        : m_progress(0)
        , m_callbackRet(0)
        , m_comId(ComInvalidId)
        , m_evtHandler(nullptr)
        , m_comSendGcodeData(comSendGcodeData)
    {
        m_materialMappings = MultiComUtils::comMaterialMappings2Fnet(m_comSendGcodeData.materialMappings);
        m_sendGcodeData.gcodeFilePath = m_comSendGcodeData.gcodeFilePath.c_str();
        m_sendGcodeData.thumbFilePath = m_comSendGcodeData.thumbFilePath.c_str();
        m_sendGcodeData.gcodeDstName = m_comSendGcodeData.gcodeDstName.c_str();
        m_sendGcodeData.printNow = m_comSendGcodeData.printNow;
        m_sendGcodeData.levelingBeforePrint = m_comSendGcodeData.levelingBeforePrint;
        m_sendGcodeData.flowCalibration = m_comSendGcodeData.flowCalibration;
        m_sendGcodeData.firstLayerInspection = m_comSendGcodeData.firstLayerInspection;
        m_sendGcodeData.timeLapseVideo = m_comSendGcodeData.timeLapseVideo;
        m_sendGcodeData.useMatlStation = m_comSendGcodeData.useMatlStation;
        m_sendGcodeData.gcodeToolCnt = (int)m_comSendGcodeData.materialMappings.size();
        m_sendGcodeData.materialMappings = m_materialMappings.data();
        m_sendGcodeData.callback = callback;
        m_sendGcodeData.callbackData = this;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        int ret;
        if (data.connectMode == COM_CONNECT_LAN) {
            ret = data.networkIntfc->lanDevSendGcode(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_sendGcodeData, ComTimeoutLanB);
        } else {
            ret = FNET_ERROR;
        }
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
    fnet_send_gcode_data_t  m_sendGcodeData;
    com_send_gcode_data_t   m_comSendGcodeData;
    std::vector<fnet_material_mapping_t> m_materialMappings;
};

class ComTempCtrl : public ComCommand
{
public:
    ComTempCtrl(double platformTemp, double rightTemp, double leftTemp, double chamberTemp)
    {
        m_tempCtrl.platformTemp = platformTemp;
        m_tempCtrl.rightTemp = rightTemp;
        m_tempCtrl.leftTemp = leftTemp;
        m_tempCtrl.chamberTemp = chamberTemp;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevTemp(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_tempCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendTempCtrl(data.nimAccountId, m_tempCtrl);
        }
    }

private:
    fnet_temp_ctrl_t m_tempCtrl;
};

class ComLightCtrl : public ComCommand
{
public:
    ComLightCtrl(const std::string &lightStatus)
        : m_lightStatus(lightStatus)
    {
        m_lightCtrl.lightStatus = m_lightStatus.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevLight(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_lightCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendLightCtrl(data.nimAccountId, m_lightCtrl);
        }
    }

private:
    std::string m_lightStatus;
    fnet_light_ctrl_t m_lightCtrl;
};

class ComAirFilterCtrl : public ComCommand
{
public:
    ComAirFilterCtrl(const std::string &internalFanStatus, const std::string &externalFanStatus)
        : m_internalFanStatus(internalFanStatus)
        , m_externalFanStatus(externalFanStatus)
    {
        m_airFilterCtrl.internalFanStatus = m_internalFanStatus.c_str();
        m_airFilterCtrl.externalFanStatus = m_externalFanStatus.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevAirFilter(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_airFilterCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendAirFilterCtrl(data.nimAccountId, m_airFilterCtrl);
        }
    }

private:
    std::string m_internalFanStatus;
    std::string m_externalFanStatus;
    fnet_air_filter_ctrl_t m_airFilterCtrl;
};

class ComClearFanCtrl : public ComCommand
{
public:
    ComClearFanCtrl(const std::string &clearFanStatus)
        : m_clearFanStatus(clearFanStatus)
    {
        m_clearFanCtrl.clearFanStatus = m_clearFanStatus.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevClearFan(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_clearFanCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendClearFanCtrl(data.nimAccountId, m_clearFanCtrl);
        }
    }

private:
    std::string m_clearFanStatus;
    fnet_clear_fan_ctrl_t m_clearFanCtrl;
};

class ComMoveCtrl : public ComCommand
{
public:
    ComMoveCtrl(const std::string &axis, double delta)
        : m_axis(axis)
    {
        m_moveCtrl.delta = delta;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevMove(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_moveCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendMoveCtrl(data.nimAccountId, m_moveCtrl);
        }
    }

private:
    std::string m_axis;
    fnet_move_ctrl_t m_moveCtrl;
};

class ComHomingCtrl : public ComCommand
{
public:
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevHoming(data.ip, data.port, data.serialNumber,
                data.checkCode, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendHomingCtrl(data.nimAccountId);
        }
    }
};

class ComMatlStationCtrl : public ComCommand
{
public:
    ComMatlStationCtrl(int slotId, int action)
    {
        m_matlStationCtrl.slotId = slotId;
        m_matlStationCtrl.action = action;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevMatlStation(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_matlStationCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendMatlStationCtrl(data.nimAccountId, m_matlStationCtrl);
        }
    }

private:
    fnet_matl_station_ctrl_t m_matlStationCtrl;
};

class ComIndepMatlCtrl : public ComCommand
{
public:
    ComIndepMatlCtrl(int action)
    {
        m_indepMatlCtrl.action = action;
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevIndepMatl(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_indepMatlCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendIndepMatlCtrl(data.nimAccountId, m_indepMatlCtrl);
        }
    }

private:
    fnet_indep_matl_ctrl_t m_indepMatlCtrl;
};

class ComPrintCtrl : public ComCommand
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
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevPrint(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_printCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendPrintCtrl(data.nimAccountId, m_printCtrl);
        }
    }

private:
    fnet_print_ctrl_t m_printCtrl;
};

class ComJobCtrl : public ComCommand
{
public:
    ComJobCtrl(const std::string &jobId, const std::string &action)
        : m_jobId(jobId)
        , m_action(action)
    {
        m_jobCtrl.jobId = m_jobId.c_str();
        m_jobCtrl.action = m_action.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevJob(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_jobCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendJobCtrl(data.nimAccountId, m_jobCtrl);
        }
    }

private:
    std::string m_jobId;
    std::string m_action;
    fnet_job_ctrl_t m_jobCtrl;
};

class ComStateCtrl : public ComCommand
{
public:
    ComStateCtrl(const std::string &action)
        : m_action(action)
    {
        m_stateCtrl.action = m_action.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevState(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_stateCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendStateCtrl(data.nimAccountId, m_stateCtrl);
        }
    }

private:
    std::string m_action;
    fnet_state_ctrl_t m_stateCtrl;
};

class ComPlateDetectCtrl : public ComCommand
{
public:
    ComPlateDetectCtrl(const std::string &action)
        : m_action(action)
    {
        m_plateDetectCtrl.action = m_action.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevPlateDetect(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_plateDetectCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendPlateDetectCtrl(data.nimAccountId, m_plateDetectCtrl);
        }
    }

private:
    std::string m_action;
    fnet_plate_detect_ctrl_t m_plateDetectCtrl;
};

class ComFirstLayerDetectCtrl : public ComCommand
{
public:
    ComFirstLayerDetectCtrl(const std::string &action)
        : m_action(action)
    {
        m_firstLayerDetectCtrl.action = m_action.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->ctrlLanDevFirstLayerDetect(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_firstLayerDetectCtrl, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendFirstLayerDetectCtrl(data.nimAccountId, m_firstLayerDetectCtrl);
        }
    }

private:
    std::string m_action;
    fnet_first_layer_detect_ctrl_t m_firstLayerDetectCtrl;
};

class ComCameraStreamCtrl : public ComCommand
{
public:
    ComCameraStreamCtrl(const std::string &action)
        : m_action(action)
    {
        m_cameraStreamCtrl.action = m_action.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            return COM_ERROR;
        } else {
            return ComWanNimConn::inst()->sendCameraStreamCtrl(data.nimAccountId, m_cameraStreamCtrl);
        }
    }

private:
    std::string m_action;
    fnet_camera_stream_ctrl_t m_cameraStreamCtrl;
};

class ComMatlStationConfig : public ComCommand
{
public:
    ComMatlStationConfig(int slotId, const std::string &materialName, const std::string &materialColor)
        : m_materialName(materialName)
        , m_materialColor(materialColor)
    {
        m_matlStationConfig.slotId = slotId;
        m_matlStationConfig.materialName = m_materialName.c_str();
        m_matlStationConfig.materialColor = m_materialColor.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->configLanDevMatlStation(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_matlStationConfig, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendMatlStationConfig(data.nimAccountId, m_matlStationConfig);
        }
    }

private:
    std::string m_materialName;
    std::string m_materialColor;
    fnet_matl_station_config_t m_matlStationConfig;
};

class ComIndepMatlConfig : public ComCommand
{
public:
    ComIndepMatlConfig(const std::string &materialName, const std::string &materialColor)
        : m_materialName(materialName)
        , m_materialColor(materialColor)
    {
        m_indepMatlConfig.materialName = m_materialName.c_str();
        m_indepMatlConfig.materialColor = m_materialColor.c_str();
    }
    ComErrno exec(const com_command_exec_data_t &data)
    {
        if (data.connectMode == COM_CONNECT_LAN) {
            int ret = data.networkIntfc->configLanDevIndepMatl(data.ip, data.port, data.serialNumber,
                data.checkCode, &m_indepMatlConfig, ComTimeoutLanA);
            return MultiComUtils::fnetRet2ComErrno(ret);
        } else {
            return ComWanNimConn::inst()->sendIndepMatlConfig(data.nimAccountId, m_indepMatlConfig);
        }

    }

private:
    std::string m_materialName;
    std::string m_materialColor;
    fnet_indep_matl_config_t m_indepMatlConfig;
};

}} // namespace Slic3r::GUI

#endif
