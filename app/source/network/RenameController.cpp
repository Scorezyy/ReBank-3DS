#include "network/RenameController.hpp"

bool RenameController::begin(
    ApiClient& api,
    std::uint16_t position,
    std::string name,
    std::string accessToken
) {
    ApiClient* apiPtr = &api;
    return task_.start([apiPtr, position, name = std::move(name), accessToken = std::move(accessToken)]() {
        Job job;
        job.position = position;
        job.result = apiPtr->renameBox(position, name, accessToken);
        return job;
    });
}

bool RenameController::poll(std::uint16_t& position, RenameBoxResult& result) {
    Job job;
    if (!task_.poll(job)) {
        return false;
    }
    position = job.position;
    result = std::move(job.result);
    return true;
}
