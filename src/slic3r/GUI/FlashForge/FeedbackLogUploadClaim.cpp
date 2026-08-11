#include "FeedbackLogUploadClaim.hpp"

#include <fstream>
#include <system_error>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace Slic3r { namespace GUI {

namespace {

std::string claim_key(const std::string& accountScope, const std::string& feedbackId)
{
    static const boost::uuids::uuid feedbackLogNamespace = boost::uuids::string_generator()("d04ff25b-5e54-4f70-8788-c1fc18240819");

    std::string value;
    value.reserve(accountScope.size() + feedbackId.size() + 1);
    value.append(accountScope);
    value.push_back('\0');
    value.append(feedbackId);
    return boost::uuids::to_string(boost::uuids::name_generator_sha1(feedbackLogNamespace)(value));
}

} // namespace

FeedbackLogUploadClaim::FeedbackLogUploadClaim(const std::string& dataDir, const std::string& accountScope, const std::string& feedbackId)
{
    const std::filesystem::path stateDir = std::filesystem::u8path(dataDir) / "feedback_log_upload_state";
    const std::string           key      = claim_key(accountScope, feedbackId);
    m_lockPath                           = stateDir / (key + ".lock");
    m_completedPath                      = stateDir / (key + ".done");
}

FeedbackLogUploadClaim::~FeedbackLogUploadClaim() { unlock(); }

FeedbackLogUploadClaim::Status FeedbackLogUploadClaim::tryAcquire()
{
    if (m_locked || m_fileLock != nullptr) {
        return Status::Error;
    }

    try {
        std::error_code error;
        std::filesystem::create_directories(m_lockPath.parent_path(), error);
        if (error) {
            return Status::Error;
        }

        // file_lock requires an existing file. Append mode creates it without
        // truncating a lock file another process may already be using.
        std::ofstream lockFile(m_lockPath, std::ios::binary | std::ios::app);
        if (!lockFile) {
            return Status::Error;
        }
        lockFile.close();

        m_fileLock = std::make_unique<boost::interprocess::file_lock>(m_lockPath.c_str());
        if (!m_fileLock->try_lock()) {
            m_fileLock.reset();
            return Status::Busy;
        }
        m_locked = true;

        const std::filesystem::file_status completedStatus = std::filesystem::status(m_completedPath, error);
        if ((!error && completedStatus.type() == std::filesystem::file_type::not_found) ||
            error == std::errc::no_such_file_or_directory) {
            return Status::Acquired;
        }
        if (error || !std::filesystem::is_regular_file(completedStatus)) {
            unlock();
            return Status::Error;
        }
        unlock();
        return Status::Completed;
    } catch (...) {
        unlock();
        return Status::Error;
    }
}

bool FeedbackLogUploadClaim::markCompleted()
{
    if (!m_locked) {
        return false;
    }

    try {
        std::ofstream completedFile(m_completedPath, std::ios::binary | std::ios::trunc);
        if (!completedFile) {
            return false;
        }
        completedFile << "completed\n";
        completedFile.flush();
        return completedFile.good();
    } catch (...) {
        return false;
    }
}

void FeedbackLogUploadClaim::unlock() noexcept
{
    if (m_locked && m_fileLock != nullptr) {
        try {
            m_fileLock->unlock();
        } catch (...) {}
    }
    m_locked = false;
    m_fileLock.reset();
}

}} // namespace Slic3r::GUI
