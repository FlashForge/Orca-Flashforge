#ifndef slic3r_GUI_FeedbackLogUploadClaim_hpp_
#define slic3r_GUI_FeedbackLogUploadClaim_hpp_

#include <filesystem>
#include <memory>
#include <string>

namespace boost { namespace interprocess {
class file_lock;
}} // namespace boost::interprocess

namespace Slic3r { namespace GUI {

class FeedbackLogUploadClaim
{
public:
    enum class Status { Acquired, Busy, Completed, Error };

    FeedbackLogUploadClaim(const std::string& dataDir, const std::string& accountScope, const std::string& feedbackId);
    ~FeedbackLogUploadClaim();

    FeedbackLogUploadClaim(const FeedbackLogUploadClaim&)            = delete;
    FeedbackLogUploadClaim& operator=(const FeedbackLogUploadClaim&) = delete;

    Status tryAcquire();
    bool   markCompleted();

private:
    void unlock() noexcept;

private:
    std::filesystem::path                           m_lockPath;
    std::filesystem::path                           m_completedPath;
    std::unique_ptr<boost::interprocess::file_lock> m_fileLock;
    bool                                            m_locked{false};
};

}} // namespace Slic3r::GUI

#endif
