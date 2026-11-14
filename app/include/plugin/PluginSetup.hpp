#pragma once

#include "network/api/ReleaseApi.hpp"
#include "platform/AsyncTask.hpp"
#include "plugin/PluginGames.hpp"
#include "plugin/PluginInstaller.hpp"
#include "plugin/PluginLoader.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

struct PluginJobResult {
    PluginKind kind = PluginKind::LiveBox;
    std::string version;
    PluginInstallOutcome outcome = PluginInstallOutcome::Unreachable;
};

struct PluginInstallRun {
    std::vector<PluginJobResult> results;
    PluginLoaderState loader = PluginLoaderState::Manual;

    bool allInstalled() const;
    bool anyInstalled() const;
};

class PluginSetup {
public:
    explicit PluginSetup(ReleaseApi& api) : api_(api) {}

    void loadCatalog(const std::string& accountId, const std::string& accessToken);
    bool pollCatalog();
    bool catalogLoaded() const { return catalog_.success; }
    bool catalogRunning() const { return catalogTask_.running(); }
    const CatalogPlugin* find(PluginKind kind) const;
    bool installable(PluginKind kind) const;

    bool install(const std::vector<std::size_t>& gameIndices, const std::string& accessToken);
    bool pollInstall();
    const PluginInstallRun& lastRun() const { return run_; }
    void clearRun() { run_ = {}; }

    const std::vector<PluginKind>& jobKinds() const { return jobKinds_; }
    std::size_t currentJob() const { return currentJob_.load(); }
    std::uint32_t received() const { return progress_.received.load(); }
    std::uint32_t total() const { return progress_.total.load(); }

private:
    void startCatalogTask();
    PluginInstallRun runJobs(const std::string& accessToken, std::vector<PluginInstallJob> jobs);

    ReleaseApi& api_;
    PluginCatalog catalog_;
    std::string catalogAccount_;
    std::string catalogToken_;
    bool discardCatalog_ = false;
    AsyncTask<PluginCatalog> catalogTask_;
    AsyncTask<PluginInstallRun> installTask_;
    PluginInstallRun run_;
    std::vector<PluginKind> jobKinds_;
    std::atomic<std::uint32_t> currentJob_{0};
    PluginInstallProgress progress_;
};
