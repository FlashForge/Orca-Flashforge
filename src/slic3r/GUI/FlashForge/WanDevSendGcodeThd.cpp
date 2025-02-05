#include "WanDevSendGcodeThd.hpp"
#include <fstream>
#include <iterator>
#include <boost/algorithm/hex.hpp>
#include <openssl/md5.h>
#include "ComCommand.hpp"
#include "FreeInDestructor.h"
#include "MultiComUtils.hpp"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

WanDevSendGcodeThd::WanDevSendGcodeThd(fnet::FlashNetworkIntfc *networkIntfc)
    : m_exitThread(false)
    , m_thread(boost::bind(&WanDevSendGcodeThd::run, this))
    , m_networkIntfc(networkIntfc)
{
}

void WanDevSendGcodeThd::exit()
{
    abortSendGcode();
    m_exitThread = true;
    m_sendGcodeEvent.set(true);
    m_thread.join();
}

bool WanDevSendGcodeThd::startSendGcode(const std::string &uid,
    const std::vector<std::string> &devIds, const std::string &gcodeFilePath,
    const std::string &thumbFilePath, const std::string &gcodeDstName, bool printNow,
    bool levelingBeforePrint)
{
    if (m_sendGcodeEvent.get()) {
        return false;
    }
    m_uid = uid;
    m_devIds = devIds;
    m_gcodeFilePath = gcodeFilePath;
    m_thumbFilePath = thumbFilePath;
    m_gcodeDstName = gcodeDstName;
    m_sendGcodeData.gcodeFilePath = m_gcodeFilePath.c_str();
    m_sendGcodeData.thumbFilePath = m_thumbFilePath.c_str();
    m_sendGcodeData.gcodeDstName = m_gcodeDstName.c_str();
    m_sendGcodeData.printNow = printNow;
    m_sendGcodeData.levelingBeforePrint = levelingBeforePrint;
    m_sendGcodeData.callback = callback;
    m_sendGcodeData.callbackData = this;
    m_progress = 0.0;
    m_callbackRet = 0;
    m_sendGcodeEvent.set(true);
    return true;
}

bool WanDevSendGcodeThd::abortSendGcode()
{
    if (!m_sendGcodeEvent.get()) {
        return false;
    }
    m_callbackRet = 1;
    return true;
}

void WanDevSendGcodeThd::run()
{
    while (!m_exitThread) {
        m_sendGcodeEvent.waitTrue();
        if (!m_exitThread) {
            ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
            const char *accessToken = token.accessToken().c_str();

            fnet_clound_gcode_data_t *cloundGcodeData;
            int fnetRet = m_networkIntfc->wanDevSendGcodeClound(
                m_uid.c_str(), accessToken, &m_sendGcodeData, &cloundGcodeData, 15000);
            fnet::FreeInDestructor freeCloundGcodeData(
                cloundGcodeData, m_networkIntfc->freeCloundGcodeData);

            fnet_clound_job_error_t *errors = nullptr;
            int errorCnt = 0;
            if (fnetRet == FNET_OK) {
                fnetRet = startCloundJob(accessToken, cloundGcodeData, &errors, &errorCnt);
            }
            fnet::FreeInDestructorArg freeErrors(errors, m_networkIntfc->freeCloudJobErrors, errorCnt);
            
            ComErrno ret = MultiComUtils::fnetRet2ComErrno(fnetRet);
            QueueEvent(new ComSendGcodeFinishEvent(COM_SEND_GCODE_FINISH_EVENT, errors, errorCnt, ret));
        }
        m_sendGcodeEvent.set(false);
    }
}

int WanDevSendGcodeThd::startCloundJob(const char *accessToken,
    const fnet_clound_gcode_data_t *cloundGcodeData, fnet_clound_job_error_t **errors, int *errorCnt)
{
    std::vector<const char *> devIds;
    for (auto &devId : m_devIds) {
        devIds.push_back(devId.c_str());
    }
    boost::filesystem::path gcodeDstPath(m_sendGcodeData.gcodeDstName);
    boost::filesystem::path gcodePath(m_sendGcodeData.gcodeFilePath);
    boost::filesystem::path thumbPath(m_sendGcodeData.thumbFilePath);
    std::string gcodeType = gcodePath.extension().string().substr(1);
    std::string gcodeMd5 = getFileMd5(m_sendGcodeData.gcodeFilePath);
    std::string thumbName = gcodeDstPath.replace_extension(".").string() + gcodeType;
    std::string thumbType = thumbPath.extension().string().substr(1);
    std::string thumbMd5 = getFileMd5(m_sendGcodeData.thumbFilePath);

    fnet_clound_job_data_t jobData;
    jobData.devIds = devIds.data();
    jobData.devCnt = devIds.size();
    jobData.gcodeName = m_sendGcodeData.gcodeDstName;
    jobData.gcodeType = gcodeType.c_str();
    jobData.gcodeMd5 = gcodeMd5.c_str();
    jobData.gcodeSize = boost::filesystem::file_size(m_sendGcodeData.gcodeFilePath);
    jobData.thumbName = thumbName.c_str();
    jobData.thumbType = thumbType.c_str();
    jobData.thumbMd5 = thumbMd5.c_str();
    jobData.thumbSize = boost::filesystem::file_size(m_sendGcodeData.thumbFilePath);;
    jobData.bucketName = cloundGcodeData->bucketName;
    jobData.endpoint = cloundGcodeData->endpoint;
    jobData.gcodeStorageKey = cloundGcodeData->gcodeStorageKey;
    jobData.gcodeStorageUrl = cloundGcodeData->gcodeStorageUrl;
    jobData.thumbStorageKey = cloundGcodeData->thumbStorageKey;
    jobData.thumbStorageUrl = cloundGcodeData->thumbStorageUrl;
    jobData.printNow = m_sendGcodeData.printNow;
    jobData.levelingBeforePrint = m_sendGcodeData.levelingBeforePrint;

    return m_networkIntfc->wanDevStartCloundJob(
        m_uid.c_str(), accessToken, &jobData, errors, errorCnt, ComTimeoutWan);
}

std::string WanDevSendGcodeThd::getFileMd5(const char *filePath)
{
    std::ifstream fs;
    fs.open(filePath, std::fstream::binary);
    if (!fs.is_open()) {
        return std::string();
    }
    MD5_CTX md5;
    if (MD5_Init(&md5) == 0) {
        return std::string();
    }
    std::vector<char> buf(4096);
    do {
        fs.read(buf.data(), buf.size());
        if (fs.gcount() > 0) {
            MD5_Update(&md5, buf.data(), fs.gcount());
        }
    } while (fs.good());

    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5);

    std::string result;
    boost::algorithm::hex_lower(std::begin(hash), std::end(hash), std::back_inserter(result));
    return result;
}

int WanDevSendGcodeThd::callback(long long now, long long total, void *callbackData)
{
    WanDevSendGcodeThd *inst = (WanDevSendGcodeThd *)callbackData;
    if (total != 0) {
        double progress = (double)now / total;
        if (progress - inst->m_progress > 0.025) {
            inst->QueueEvent(new ComSendGcodeProgressEvent(COM_SEND_GCODE_PROGRESS_EVENT, now, total));
            inst->m_progress = progress;
        }
    }
    return inst->m_callbackRet;
}

}} // namespace Slic3r::GUI
