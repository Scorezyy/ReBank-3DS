#include "bank/CommitPlan.hpp"

#include <algorithm>
#include <numeric>
#include <set>
#include <utility>

namespace {
using PayloadKey = std::pair<std::uint8_t, std::vector<std::uint8_t>>;

PayloadKey keyOf(const PokemonPayload& payload) {
    return {payload.format, payload.data};
}

struct SlotNode {
    SlotRef ref;
    PokemonSummary baseSummary;
    PokemonPayload basePayload;
    PokemonSummary currentSummary;
    PokemonPayload currentPayload;
    std::optional<std::size_t> matchedArrival;
    bool deleted = false;
    std::optional<IssueReason> conflict;

    bool departs() const { return baseSummary.occupied(); }
    bool arrives() const { return currentSummary.occupied(); }
};

class DisjointSet {
public:
    explicit DisjointSet(std::size_t size) : parent_(size) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }

    std::size_t find(std::size_t index) {
        while (parent_[index] != index) {
            parent_[index] = parent_[parent_[index]];
            index = parent_[index];
        }
        return index;
    }

    void unite(std::size_t a, std::size_t b) { parent_[find(a)] = find(b); }

private:
    std::vector<std::size_t> parent_;
};

template <typename Map>
std::vector<typename Map::key_type> sortedKeys(const Map& map) {
    std::vector<typename Map::key_type> keys;
    keys.reserve(map.size());
    for (const auto& entry : map) {
        keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

SlotNode makeNode(SlotRef ref, const PokemonSummary& baseSummary, const PokemonPayload& basePayload,
                  const PokemonSummary& currentSummary, const PokemonPayload& currentPayload) {
    SlotNode node;
    node.ref = ref;
    node.baseSummary = baseSummary;
    node.basePayload = basePayload;
    node.currentSummary = currentSummary;
    node.currentPayload = currentPayload;
    return node;
}

std::vector<SlotNode> collectChangedSlots(const CommitSnapshot& snapshot) {
    std::vector<SlotNode> nodes;
    for (const std::size_t box : sortedKeys(snapshot.localDrafts)) {
        const BoxSlots& draft = snapshot.localDrafts.at(box);
        const auto baseline = snapshot.localBaselines.find(box);
        if (baseline == snapshot.localBaselines.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < BoxSlotCount; ++slot) {
            if (draft.slotDiffers(baseline->second, slot)) {
                nodes.push_back(makeNode(SlotRef::local(box, slot), baseline->second.summaries[slot],
                                         baseline->second.payloads[slot], draft.summaries[slot],
                                         draft.payloads[slot]));
            }
        }
    }
    for (std::size_t slot = 0; slot < PartySlotCount; ++slot) {
        if (snapshot.partyWorking.slotDiffers(snapshot.partyBaseline, slot)) {
            nodes.push_back(makeNode(SlotRef::party(slot), snapshot.partyBaseline.summaries[slot],
                                     snapshot.partyBaseline.payloads[slot], snapshot.partyWorking.summaries[slot],
                                     snapshot.partyWorking.payloads[slot]));
        }
    }
    for (const std::uint16_t key : sortedKeys(snapshot.cloudBoxes)) {
        const CloudBoxDraft& draft = snapshot.cloudBoxes.at(key);
        for (std::size_t slot = 0; slot < BoxSlotCount; ++slot) {
            if (!draft.slotChanged(slot)) {
                continue;
            }
            const PokemonPayload current = draft.pending[slot].known() ? draft.pending[slot] : PokemonPayload{};
            nodes.push_back(makeNode(SlotRef::cloud(key, slot), draft.baseline[slot], draft.payloads[slot],
                                     draft.summaries[slot], current));
        }
    }
    return nodes;
}

void matchArrivals(std::vector<SlotNode>& nodes, DisjointSet& components) {
    std::map<PayloadKey, std::vector<std::size_t>> departures;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        SlotNode& node = nodes[index];
        if (!node.departs()) {
            continue;
        }
        if (!node.basePayload.known()) {
            node.conflict = IssueReason::DataNotLoaded;
            continue;
        }
        departures[keyOf(node.basePayload)].push_back(index);
    }
    for (auto& entry : departures) {
        std::reverse(entry.second.begin(), entry.second.end());
    }
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        SlotNode& node = nodes[index];
        if (!node.arrives()) {
            continue;
        }
        const auto candidates = node.currentPayload.known()
            ? departures.find(keyOf(node.currentPayload)) : departures.end();
        if (candidates == departures.end() || candidates->second.empty()) {
            node.conflict = IssueReason::OriginUnconfirmed;
            continue;
        }
        const std::size_t departure = candidates->second.back();
        candidates->second.pop_back();
        nodes[departure].matchedArrival = index;
        components.unite(departure, index);
    }
}

void markDeletions(std::vector<SlotNode>& nodes, const std::vector<PokemonPayload>& confirmedDeletions) {
    std::vector<bool> consumed(confirmedDeletions.size(), false);
    for (SlotNode& node : nodes) {
        if (!node.departs() || !node.basePayload.known() || node.matchedArrival) {
            continue;
        }
        node.conflict = IssueReason::NotInTrash;
        for (std::size_t index = 0; index < confirmedDeletions.size(); ++index) {
            if (!consumed[index] && confirmedDeletions[index] == node.basePayload) {
                consumed[index] = true;
                node.deleted = true;
                node.conflict.reset();
                break;
            }
        }
    }
}

TransferKind classify(const SlotRef& origin, const SlotRef& destination) {
    if (origin.isCloud()) {
        return destination.isCloud() ? TransferKind::CloudMove : TransferKind::Download;
    }
    return destination.isCloud() ? TransferKind::Upload : TransferKind::LocalMove;
}

class Planner {
public:
    Planner(const CommitSnapshot& snapshot, const LocalSlotProbe& probe)
        : snapshot_(snapshot), probe_(probe), nodes_(collectChangedSlots(snapshot)), components_(nodes_.size()) {}

    CommitPlan run() {
        matchArrivals(nodes_, components_);
        markDeletions(nodes_, snapshot_.confirmedDeletions);
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            if (nodes_[index].conflict) {
                frozen_.insert(components_.find(index));
            }
        }
        for (const SlotNode& node : nodes_) {
            changedSlots_.insert(node.ref);
        }
        CommitPlan plan;
        while (true) {
            plan = CommitPlan{};
            buildTransfers(plan);
            if (assignStaging(plan)) {
                break;
            }
        }
        buildLayouts(plan);
        simulateCloud(plan);
        reportHeldBack(plan);
        return plan;
    }

private:
    bool isFrozen(std::size_t node) { return frozen_.count(components_.find(node)) != 0; }

    void buildTransfers(CommitPlan& plan) {
        nodeOfTransfer_.clear();
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            const SlotNode& node = nodes_[index];
            if (!node.departs() || node.conflict || isFrozen(index)) {
                continue;
            }
            PlannedTransfer transfer;
            transfer.origin = node.ref;
            transfer.group = components_.find(index);
            if (node.matchedArrival) {
                const SlotNode& arrival = nodes_[*node.matchedArrival];
                transfer.kind = classify(node.ref, arrival.ref);
                transfer.destination = arrival.ref;
                transfer.summary = arrival.currentSummary;
                transfer.payload = arrival.currentPayload;
            } else if (node.deleted) {
                transfer.kind = node.ref.isCloud() ? TransferKind::CloudDelete : TransferKind::LocalDelete;
                transfer.destination = node.ref;
                transfer.summary = node.baseSummary;
                transfer.payload = node.basePayload;
            } else {
                continue;
            }
            plan.transfers.push_back(std::move(transfer));
            nodeOfTransfer_.push_back(index);
        }
    }

    bool stagingCandidate(const SlotRef& ref, const std::set<SlotRef>& taken) const {
        if (changedSlots_.count(ref) || taken.count(ref)) {
            return false;
        }
        const auto baseline = snapshot_.localBaselines.find(ref.box);
        if (baseline != snapshot_.localBaselines.end()) {
            return !baseline->second.summaries[ref.slot].occupied();
        }
        return probe_.occupied && !probe_.occupied(ref.box, ref.slot);
    }

    std::optional<SlotRef> findStagingSlot(const PlannedTransfer& transfer, const std::set<SlotRef>& taken) const {
        if (probe_.boxCount == 0) {
            return std::nullopt;
        }
        std::size_t startBox = 0;
        if (transfer.destination.kind == SlotKind::Local) {
            startBox = transfer.destination.box;
        } else if (transfer.origin.kind == SlotKind::Local) {
            startBox = transfer.origin.box;
        }
        for (std::size_t offset = 0; offset < probe_.boxCount; ++offset) {
            const std::size_t box = (startBox + offset) % probe_.boxCount;
            for (std::size_t slot = 0; slot < std::min(probe_.slotsPerBox, BoxSlotCount); ++slot) {
                const SlotRef candidate = SlotRef::local(box, slot);
                if (stagingCandidate(candidate, taken)) {
                    return candidate;
                }
            }
        }
        return std::nullopt;
    }

    bool assignStaging(CommitPlan& plan) {
        std::set<SlotRef> uploadOrigins;
        for (const PlannedTransfer& transfer : plan.transfers) {
            if (transfer.kind == TransferKind::Upload) {
                uploadOrigins.insert(transfer.origin);
            }
        }
        std::set<SlotRef> taken;
        std::set<std::size_t> failedComponents;
        for (std::size_t index = 0; index < plan.transfers.size(); ++index) {
            PlannedTransfer& transfer = plan.transfers[index];
            if (!transfer.landsLocally() || !uploadOrigins.count(transfer.destination)) {
                continue;
            }
            const std::optional<SlotRef> staging = findStagingSlot(transfer, taken);
            if (!staging) {
                nodes_[nodeOfTransfer_[index]].conflict = IssueReason::NoParkingSlot;
                failedComponents.insert(components_.find(nodeOfTransfer_[index]));
                continue;
            }
            taken.insert(*staging);
            transfer.staging = staging;
        }
        if (failedComponents.empty()) {
            return true;
        }
        frozen_.insert(failedComponents.begin(), failedComponents.end());
        return false;
    }

    void buildLayouts(CommitPlan& plan) {
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            if (nodes_[index].ref.isCloud() || isFrozen(index)) {
                continue;
            }
            plan.baselineLayout[nodes_[index].ref] = std::nullopt;
            plan.stagedLayout[nodes_[index].ref] = std::nullopt;
        }
        for (std::size_t index = 0; index < plan.transfers.size(); ++index) {
            const PlannedTransfer& transfer = plan.transfers[index];
            if (!transfer.origin.isCloud()) {
                plan.baselineLayout[transfer.origin] = index;
            }
            if (transfer.kind == TransferKind::Upload) {
                plan.stagedLayout[transfer.origin] = index;
            } else if (transfer.landsLocally()) {
                if (transfer.staging) {
                    plan.baselineLayout.emplace(*transfer.staging, std::nullopt);
                    plan.stagedLayout[*transfer.staging] = index;
                } else {
                    plan.stagedLayout[transfer.destination] = index;
                }
            }
        }
    }

    void simulateCloud(CommitPlan& plan) const {
        std::map<SlotRef, std::size_t> server;
        std::map<std::size_t, SlotRef> position;
        for (std::size_t index = 0; index < plan.transfers.size(); ++index) {
            const PlannedTransfer& transfer = plan.transfers[index];
            if (transfer.origin.isCloud()) {
                server[transfer.origin] = index;
                position[index] = transfer.origin;
            }
        }
        for (std::size_t index = 0; index < plan.transfers.size(); ++index) {
            const PlannedTransfer& transfer = plan.transfers[index];
            if (transfer.kind != TransferKind::CloudMove) {
                continue;
            }
            const SlotRef from = position[index];
            const SlotRef to = transfer.destination;
            if (from == to) {
                continue;
            }
            plan.cloudMoves.push_back({from, to});
            const auto occupant = server.find(to);
            const std::optional<std::size_t> displaced = occupant != server.end()
                ? std::optional<std::size_t>(occupant->second) : std::nullopt;
            server[to] = index;
            position[index] = to;
            if (displaced) {
                server[from] = *displaced;
                position[*displaced] = from;
            } else {
                server.erase(from);
            }
        }
        for (std::size_t index = 0; index < plan.transfers.size(); ++index) {
            PlannedTransfer& transfer = plan.transfers[index];
            if (transfer.kind == TransferKind::Download || transfer.kind == TransferKind::CloudDelete) {
                transfer.cloudSlotAfterMoves = position[index];
            } else if (transfer.kind == TransferKind::Upload) {
                const auto occupant = server.find(transfer.destination);
                if (occupant != server.end()) {
                    transfer.displaced = occupant->second;
                }
            }
        }
    }

    void reportHeldBack(CommitPlan& plan) {
        for (std::size_t index = 0; index < nodes_.size(); ++index) {
            const SlotNode& node = nodes_[index];
            if (!isFrozen(index)) {
                continue;
            }
            if (node.departs()) {
                plan.heldBack.push_back({node.baseSummary, node.ref, node.conflict.value_or(IssueReason::LinkedToHeldBack)});
            } else if (node.arrives() && node.conflict) {
                plan.heldBack.push_back({node.currentSummary, node.ref, *node.conflict});
            }
        }
    }

    const CommitSnapshot& snapshot_;
    const LocalSlotProbe& probe_;
    std::vector<SlotNode> nodes_;
    DisjointSet components_;
    std::set<std::size_t> frozen_;
    std::set<SlotRef> changedSlots_;
    std::vector<std::size_t> nodeOfTransfer_;
};
}

std::string describeSlot(const SlotRef& ref) {
    switch (ref.kind) {
        case SlotKind::Party:
            return "Party slot " + std::to_string(ref.slot + 1);
        case SlotKind::Cloud:
            return "Bank " + std::to_string(ref.box + 1) + "  |  Slot " + std::to_string(ref.slot + 1);
        case SlotKind::Local:
            break;
    }
    return "Local box " + std::to_string(ref.box + 1) + "  |  Slot " + std::to_string(ref.slot + 1);
}

const char* describeReason(IssueReason reason) {
    switch (reason) {
        case IssueReason::DataNotLoaded: return "its data was not loaded yet; nothing changed";
        case IssueReason::OriginUnconfirmed: return "its origin could not be confirmed; nothing changed";
        case IssueReason::NotInTrash: return "it would have vanished without the trash; nothing changed";
        case IssueReason::LinkedToHeldBack: return "linked to a Pokemon that could not be moved; nothing changed";
        case IssueReason::NoParkingSlot: return "no free box slot to park it during the swap; nothing changed";
        case IssueReason::CloudMoveFailed: return "the cloud did not confirm the move";
        case IssueReason::WriteFailed: return "it could not be written into this save; nothing changed";
        case IssueReason::RemoveFailed: return "it could not be removed from this save; nothing changed";
        case IssueReason::CloudRefused: return "the cloud refused it; it stayed in the save";
        case IssueReason::UploadUnconfirmed: return "upload outcome unknown; it stayed in the save, check the cloud for a copy";
        case IssueReason::UploadNotStored: return "the upload did not go through; it stayed in the save";
        case IssueReason::CloudSlotDifferent: return "the cloud slot holds a different Pokemon; it stayed in the save";
        case IssueReason::UploadUndone: return "final save failed; upload undone, it stayed in the save";
        case IssueReason::UploadNotUndone: return "final save failed and the cloud copy could not be removed; it exists twice";
        case IssueReason::SwapCancelled: return "swap partner could not be uploaded; swap cancelled";
        case IssueReason::PlacedElsewhere: return "target slot stayed occupied; placed in a free slot instead";
        case IssueReason::ReturnedToOrigin: return "target slot stayed occupied; returned to its old slot";
        case IssueReason::OverwriteUnconfirmed: return "upload into its cloud slot unconfirmed; it may still be in the cloud";
        case IssueReason::DownloadNotRemoved: return "saved in the game but not removed from the cloud; it exists twice";
        case IssueReason::CloudRemoveFailed: return "it could not be removed from the cloud";
    }
    return "unknown";
}

std::string describePokemon(const PokemonSummary& summary) {
    return summary.nickname.empty() ? "Pokemon #" + std::to_string(summary.species) : summary.nickname;
}

CommitPlan planCommit(const CommitSnapshot& snapshot, const LocalSlotProbe& probe) {
    return Planner(snapshot, probe).run();
}
