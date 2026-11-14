#include "plugin/PluginSetup.hpp"

#include "platform/Logger.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace {
bool installed(const PluginJobResult& result) {
    return result.outcome == PluginInstallOutcome::Installed;
}

std::string versionOf(const std::string& tag) {
    return tag.size() > 1 ? tag.substr(1) : tag;
}
}

bool PluginInstallRun::allInstalled() const {
    return !results.empty() && std::all_of(results.begin(), results.end(), installed);
}

bool PluginInstallRun::anyInstalled() const {
    return std::any_of(results.begin(), results.end(), installed);
}

void PluginSetup::loadCatalog(const std::string& accountId, const std::string& accessToken) {
    if (catalogAccount_ != accountId) {
        catalog_ = {};
        catalogAccount_ = accountId;
        discardCatalog_ = catalogTask_.running();
    }
    catalogToken_ = accessToken;
    if (!catalogTask_.running()) {
        startCatalogTask();
    }
}

void PluginSetup::startCatalogTask() {
    const std::string token = catalogToken_;
    if (!catalogTask_.start([this, token]() { return api_.pluginCatalog(token); })) {
        Logger::instance().warning("Plugin catalog worker creation failed");
    }
}

bool PluginSetup::pollCatalog() {
    PluginCatalog fresh;
    if (!catalogTask_.poll(fresh)) {
        return false;
    }
    if (std::exchange(discardCatalog_, false)) {
        startCatalogTask();
        return false;
    }
    if (!fresh.success) {
        Logger::instance().warning("Plugin catalog: " + fresh.message);
    }
    if (fresh.success || !catalog_.success) {
        catalog_ = std::move(fresh);
    }
    return true;
}

const CatalogPlugin* PluginSetup::find(PluginKind kind) const {
    const std::string_view id = PluginGames::catalogId(kind);
    const auto match = std::find_if(catalog_.plugins.begin(), catalog_.plugins.end(),
                                    [&](const CatalogPlugin& plugin) { return plugin.id == id; });
    return match == catalog_.plugins.end() ? nullptr : &*match;
}

bool PluginSetup::installable(PluginKind kind) const {
    const CatalogPlugin* plugin = find(kind);
    return plugin != nullptr && plugin->access && plugin->available;
}

bool PluginSetup::install(const std::vector<std::size_t>& gameIndices, const std::string& accessToken) {
    const auto games = PluginGames::all();
    std::map<PluginKind, PluginInstallJob> grouped;
    for (const std::size_t index : gameIndices) {
        const PluginGame& game = games[index];
        if (!installable(game.kind)) {
            continue;
        }
        const CatalogPlugin& plugin = *find(game.kind);
        PluginInstallJob& job = grouped[game.kind];
        job.id = plugin.id;
        job.tag = plugin.tag;
        job.file = plugin.file;
        job.fileName = std::string(PluginGames::fileName(game.kind));
        for (const std::uint64_t titleId : game.titleIds) {
            job.folders.push_back(PluginGames::folderName(titleId));
        }
    }
    std::vector<PluginInstallJob> jobs;
    jobKinds_.clear();
    for (auto& [kind, job] : grouped) {
        jobKinds_.push_back(kind);
        jobs.push_back(std::move(job));
    }
    currentJob_.store(0);
    progress_.received.store(0);
    progress_.total.store(0);
    run_ = {};
    const bool started = installTask_.start(
        [this, accessToken, jobs = std::move(jobs)]() mutable { return runJobs(accessToken, std::move(jobs)); });
    if (!started) {
        Logger::instance().warning("Plugin install worker creation failed");
    }
    return started;
}

bool PluginSetup::pollInstall() {
    return installTask_.poll(run_);
}

PluginInstallRun PluginSetup::runJobs(const std::string& accessToken, std::vector<PluginInstallJob> jobs) {
    PluginInstallRun run;
    for (std::size_t index = 0; index < jobs.size(); ++index) {
        currentJob_.store(static_cast<std::uint32_t>(index));
        const PluginInstallOutcome outcome = PluginInstaller::install(api_, accessToken, jobs[index], progress_);
        run.results.push_back({jobKinds_[index], versionOf(jobs[index].tag), outcome});
    }
    run.loader = run.anyInstalled() ? PluginLoader::enable() : PluginLoaderState::Manual;
    return run;
}
