#include "network/HttpClient.hpp"

#include "config/BuildConfig.hpp"
#include "config/ServerConfig.hpp"
#include "network/Json.hpp"
#include "platform/FileSystem.hpp"
#include "platform/Logger.hpp"
#include "security/Hex.hpp"
#include "security/RequestSigning.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace {
constexpr std::size_t MaximumResponseSize = 64 * 1024;
constexpr std::size_t MaximumRequestSize = 0xF000;
constexpr std::size_t MaximumDownloadSize = 5 * 1024 * 1024;
constexpr std::size_t DownloadChunkSize = 32 * 1024;
constexpr std::size_t MaximumCertificateSize = 16 * 1024;
constexpr int MaximumAttempts = 3;
constexpr std::uint32_t MaximumRetryWaitSeconds = 20;
constexpr std::string_view TimePath = "/v1/time";
constexpr const char* TrustedRootPaths[] = {
    "romfs:/assets/tls/usertrust-rsa.der",
    "romfs:/assets/tls/sectigo-r46.der",
    "romfs:/assets/tls/isrg-x1.der",
};
const std::string JsonContentType = "application/json";

class Connection {
public:
    Connection() = default;
    ~Connection() { close(); }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Result open(HTTPC_RequestMethod method, const std::string& url) {
        const Result result = httpcOpenContext(&context_, method, url.c_str(), 0);
        open_ = R_SUCCEEDED(result);
        return result;
    }

    void close() {
        if (std::exchange(open_, false)) {
            httpcCloseContext(&context_);
        }
    }

    httpcContext* operator->() { return &context_; }
    httpcContext* get() { return &context_; }

private:
    httpcContext context_{};
    bool open_ = false;
};

std::string resultCode(Result result) {
    return Hex::resultCode(static_cast<std::uint32_t>(result));
}

const std::vector<std::vector<u8>>& trustedRootCertificates() {
    static const std::vector<std::vector<u8>> certificates = [] {
        std::vector<std::vector<u8>> loaded;
        for (const char* path : TrustedRootPaths) {
            std::optional<std::vector<u8>> certificate = FileSystem::read(path, MaximumCertificateSize);
            if (!certificate) {
                Logger::instance().error(std::string("Trusted root missing: ") + path);
                continue;
            }
            loaded.push_back(std::move(*certificate));
        }
        return loaded;
    }();
    return certificates;
}

Result openTrusted(Connection& connection, HTTPC_RequestMethod method, const std::string& path) {
    const Result result = connection.open(method, ServerConfig::baseUrl() + path);
    if (R_FAILED(result)) {
        return result;
    }
    for (const auto& certificate : trustedRootCertificates()) {
        const Result added = httpcAddTrustedRootCA(connection.get(), certificate.data(), static_cast<u32>(certificate.size()));
        if (R_FAILED(added)) {
            Logger::instance().error("Trusted root rejected: " + resultCode(added));
        }
    }
    return result;
}

HTTPC_RequestMethod httpcMethod(HttpMethod method) {
    switch (method) {
        case HttpMethod::Post:
            return HTTPC_METHOD_POST;
        case HttpMethod::Put:
            return HTTPC_METHOD_PUT;
        case HttpMethod::Delete:
            return HTTPC_METHOD_DELETE;
        case HttpMethod::Get:
            break;
    }
    return HTTPC_METHOD_GET;
}

const char* methodName(HttpMethod method) {
    switch (method) {
        case HttpMethod::Post:
            return "POST";
        case HttpMethod::Put:
            return "PUT";
        case HttpMethod::Delete:
            return "DELETE";
        case HttpMethod::Get:
            break;
    }
    return "GET";
}

std::uint32_t retryAfterSeconds(Connection& connection) {
    char value[16]{};
    if (R_FAILED(httpcGetResponseHeader(connection.get(), "Retry-After", value, sizeof(value) - 1))) {
        return 0;
    }
    return static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10));
}

bool signatureExpired(const HttpResponse& response) {
    return response.success && response.status == 401 && response.body.find("signature has expired") != std::string::npos;
}

using Headers = std::vector<std::pair<const char*, std::string>>;

struct Exchange {
    Result result = 0;
    std::uint32_t status = 0;
    bool sent = false;
    const char* failedStage = nullptr;
    const char* failedMessage = nullptr;
};

Headers signedHeaders(HttpMethod method, const std::string& path, const std::string& body,
                      const std::string& accessToken, std::uint64_t timestampSeconds) {
    const std::string timestamp = std::to_string(timestampSeconds);
    Headers headers;
    if (!body.empty()) {
        headers.emplace_back("Content-Type", JsonContentType);
    }
    headers.emplace_back("Accept", JsonContentType);
    headers.emplace_back("X-Client-Version", BuildConfig::Version);
    headers.emplace_back("X-Client-Timestamp", timestamp);
    headers.emplace_back("X-Client-Signature", RequestSigning::sign(methodName(method), path, BuildConfig::Version,
                                                                    timestamp, body, ServerConfig::clientSecret()));
    if (!accessToken.empty()) {
        headers.emplace_back("Authorization", "Bearer " + accessToken);
    }
    return headers;
}

Exchange exchange(Connection& connection, const Headers& headers, const std::string& body,
                  std::uint64_t timeoutNanoseconds) {
    Exchange exchanged;
    for (const auto& [name, value] : headers) {
        if (R_SUCCEEDED(exchanged.result)) {
            exchanged.result = httpcAddRequestHeaderField(connection.get(), name, value.c_str());
        }
    }
    std::vector<u32> upload((body.size() + sizeof(u32) - 1) / sizeof(u32));
    if (!body.empty() && R_SUCCEEDED(exchanged.result)) {
        std::memcpy(upload.data(), body.data(), body.size());
        exchanged.result = httpcAddPostDataRaw(connection.get(), upload.data(), body.size());
    }
    if (R_FAILED(exchanged.result)) {
        exchanged.failedStage = "request setup";
        exchanged.failedMessage = "Secure connection failed";
        return exchanged;
    }
    exchanged.sent = true;
    exchanged.result = httpcBeginRequest(connection.get());
    if (R_FAILED(exchanged.result)) {
        exchanged.failedStage = "request";
        exchanged.failedMessage = "Secure connection failed";
        return exchanged;
    }
    u32 status = 0;
    exchanged.result = httpcGetResponseStatusCodeTimeout(connection.get(), &status, timeoutNanoseconds);
    if (R_FAILED(exchanged.result)) {
        exchanged.failedStage = "response";
        exchanged.failedMessage = "The server response failed";
    }
    exchanged.status = status;
    return exchanged;
}

HttpResponse failure(const char* stage, const char* message, Result result, bool sent) {
    const std::string code = resultCode(result);
    Logger::instance().error(std::string("HTTP ") + stage + " failed: " + code);
    return HttpResponse{false, 0, {}, std::string(message) + " (" + code + ").", sent};
}

HttpResponse exchangeFailure(const Exchange& exchanged) {
    return failure(exchanged.failedStage, exchanged.failedMessage, exchanged.result, exchanged.sent);
}

HttpResponse readBody(Connection& connection, std::uint32_t status) {
    const std::uint32_t retryAfter = status == 429 ? retryAfterSeconds(connection) : 0;
    u32 downloaded = 0;
    u32 contentSize = 0;
    httpcGetDownloadSizeState(connection.get(), &downloaded, &contentSize);
    if (contentSize > MaximumResponseSize) {
        return {false, status, {}, "The server response is too large.", true, retryAfter};
    }
    std::vector<u8> body(MaximumResponseSize + 1);
    const Result result = httpcReceiveData(connection.get(), body.data(), MaximumResponseSize);
    httpcGetDownloadSizeState(connection.get(), &downloaded, &contentSize);
    connection.close();
    if (R_FAILED(result) && static_cast<u32>(result) != HTTPC_RESULTCODE_DOWNLOADPENDING) {
        return failure("download", "The server response could not be downloaded", result, true);
    }
    if (downloaded > MaximumResponseSize) {
        return {false, status, {}, "The server response is too large.", true};
    }
    Logger::instance().info("HTTP " + std::to_string(status));
    return {true, status, std::string(reinterpret_cast<char*>(body.data()), downloaded), {}, true, retryAfter};
}

Result receiveInto(Connection& connection, std::FILE* file, std::uint32_t expectedSize,
                   std::atomic<std::uint32_t>& progress) {
    std::vector<u8> buffer(DownloadChunkSize);
    std::uint32_t total = 0;
    Result result = 0;
    do {
        u32 before = 0;
        u32 after = 0;
        u32 contentSize = 0;
        httpcGetDownloadSizeState(connection.get(), &before, &contentSize);
        result = httpcReceiveDataTimeout(connection.get(), buffer.data(), buffer.size(), HttpClient::DefaultTimeoutNanoseconds);
        httpcGetDownloadSizeState(connection.get(), &after, &contentSize);
        const u32 received = after >= before ? after - before : 0;
        if (received > buffer.size() || total + received > expectedSize
            || (received > 0 && std::fwrite(buffer.data(), 1, received, file) != received)) {
            return static_cast<Result>(-1);
        }
        total += received;
        progress.store(total);
        if (total == expectedSize) {
            return 0;
        }
    } while (static_cast<u32>(result) == HTTPC_RESULTCODE_DOWNLOADPENDING);
    return result;
}
}

HttpClient::HttpClient() {
    initialized_ = R_SUCCEEDED(httpcInit(0x10000));
    if (!initialized_) {
        Logger::instance().error("HTTP service initialization failed");
    }
}

HttpClient::~HttpClient() {
    if (initialized_) {
        httpcExit();
    }
}

bool HttpClient::consumeSessionRejected() {
    return sessionRejected_.exchange(false);
}

HttpResponse HttpClient::send(HttpMethod method, const std::string& path, const std::string& body,
                              const std::string& accessToken, std::uint64_t timeoutNanoseconds) {
    bool clockRetried = false;
    for (int attempt = 1;; ++attempt) {
        HttpResponse response = sendOnce(method, path, body, accessToken, timeoutNanoseconds);
        if (signatureExpired(response) && !clockRetried && path != TimePath) {
            clockRetried = true;
            Logger::instance().warning("Request signature expired for " + path + ", resyncing the clock and retrying");
            syncClock(true);
            continue;
        }
        if (!response.success || response.status != 429 || attempt >= MaximumAttempts) {
            return response;
        }
        const std::uint32_t wait = std::clamp<std::uint32_t>(response.retryAfterSeconds, 1, MaximumRetryWaitSeconds);
        Logger::instance().warning("HTTP 429 for " + path + ", retrying in " + std::to_string(wait) + "s");
        svcSleepThread(static_cast<s64>(wait) * 1'000'000'000LL);
    }
}

HttpResponse HttpClient::sendOnce(HttpMethod method, const std::string& path, const std::string& body,
                                  const std::string& accessToken, std::uint64_t timeoutNanoseconds) {
    if (!initialized_) {
        return {false, 0, {}, "The network service is unavailable."};
    }
    if (body.size() > MaximumRequestSize) {
        return {false, 0, {}, "The request could not be encoded."};
    }
    if (path != TimePath) {
        syncClock();
    }
    const Headers headers = signedHeaders(method, path, body, accessToken, clock_.unixSeconds());
    Logger::instance().info(std::string(methodName(method)) + " " + path);

    Connection connection;
    const Result opened = openTrusted(connection, httpcMethod(method), path);
    if (R_FAILED(opened)) {
        const std::string message = "Connection setup failed (" + resultCode(opened) + ").";
        Logger::instance().error("HTTP context setup failed: " + message);
        return {false, 0, {}, message};
    }
    const Exchange exchanged = exchange(connection, headers, body, timeoutNanoseconds);
    if (exchanged.failedStage) {
        return exchangeFailure(exchanged);
    }
    HttpResponse response = readBody(connection, exchanged.status);
    if (response.success && response.status == 401 && !accessToken.empty()) {
        sessionRejected_ = true;
    }
    return response;
}

FileDownloadResult HttpClient::download(const std::string& path, const std::string& destination,
                                        std::uint32_t expectedSize, std::atomic<std::uint32_t>& progress,
                                        const std::string& accessToken) {
    if (!initialized_ || expectedSize == 0 || expectedSize > MaximumDownloadSize) {
        return {false, "The download request is invalid.", 0};
    }
    progress.store(0);
    Connection connection;
    const Result opened = openTrusted(connection, HTTPC_METHOD_GET, path);
    if (R_FAILED(opened)) {
        return {false, "Connection setup failed (" + resultCode(opened) + ").", 0};
    }
    Headers headers{{"Accept", "application/octet-stream"}};
    if (!accessToken.empty()) {
        headers.emplace_back("Authorization", "Bearer " + accessToken);
    }
    const Exchange exchanged = exchange(connection, headers, {}, DefaultTimeoutNanoseconds);
    if (exchanged.failedStage || exchanged.status != 200) {
        return {false, "Download connection failed (" + resultCode(exchanged.result) + ").", 0};
    }

    std::FILE* file = std::fopen(destination.c_str(), "wb");
    if (!file) {
        return {false, "The download file could not be created.", 0};
    }
    const Result result = receiveInto(connection, file, expectedSize, progress);
    const bool closed = std::fclose(file) == 0;
    const std::uint32_t total = progress.load();
    if (R_FAILED(result) || !closed || total != expectedSize) {
        std::remove(destination.c_str());
        return {false, "The download was incomplete (" + resultCode(result) + ", " + std::to_string(total) + "/"
                    + std::to_string(expectedSize) + " bytes" + (closed ? "" : ", file not closed") + ").", total};
    }
    return {true, "Download complete.", total};
}

void HttpClient::syncClock(bool force) {
    if (!clock_.claimSync(force)) {
        return;
    }
    const std::int64_t localMsBeforeRequest = static_cast<std::int64_t>(osGetTime());
    const HttpResponse response = send(HttpMethod::Get, std::string(TimePath));
    if (!response.success || response.status != 200) {
        return;
    }
    const Json::Document root = Json::Document::parse(response.body);
    if (json_is_integer(root.field("unixSeconds"))) {
        clock_.record(localMsBeforeRequest, root.integer<std::int64_t>("unixSeconds", 0));
    }
}
