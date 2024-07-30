#pragma once

// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <memory>
#include <set>
#include <vector>
#include <tuple>
#include <map>

#include "lib/json/json-forwards.h"
#include "scp/SCPDriver.h"
#include "scp/SCP.h"
#include "util/HashOfHash.h"
#include "herder/QuorumTracker.h"

namespace stellar
{
/**
 * This is one Node in the stellar network
 */
class LocalNode
{
  protected:
    const NodeID mNodeID;
    const bool mIsValidator;
    SCPQuorumSet mQSet;
    Hash mQSetHash;
    std::set<NodeID> mTombSet;
    // all the variable related to add use <p, q_c> as index
    std::set<std::tuple<NodeID, std::vector<NodeID>>> mTentative;
    std::map<std::tuple<NodeID, std::vector<NodeID>>, std::set<NodeID>> mAck;
    std::map<std::tuple<NodeID, std::vector<NodeID>>, std::set<NodeID>> mNack;
    std::map<std::tuple<NodeID, std::vector<NodeID>>, std::set<NodeID>> mFailed;
    std::map<std::tuple<NodeID, std::vector<NodeID>>, std::set<NodeID>> mCommit;
    std::map<std::tuple<NodeID, std::vector<NodeID>>, std::set<NodeID>> mCheckAck;
    std::map<std::tuple<NodeID, std::vector<NodeID>>, std::set<NodeID>> mCheckNack;

    // alternative qset used during externalize {{mNodeID}}
    Hash gSingleQSetHash;                      // hash of the singleton qset
    std::shared_ptr<SCPQuorumSet> mSingleQSet; // {{mNodeID}}

    SCPDriver& mDriver;

  public:
    LocalNode(NodeID const& nodeID, bool isValidator, SCPQuorumSet const& qSet,
              SCPDriver& driver);

    NodeID const& getNodeID();

    void updateQuorumSet(SCPQuorumSet const& qSet);
    void updateTombSet(std::set<NodeID> tSet);
    void addTentative(std::tuple<NodeID, std::vector<NodeID>> addT);
    void removeTentative(std::tuple<NodeID, std::vector<NodeID>> removeT);
    void addAck(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender);
    void addNack(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender);
    void removeAck(std::tuple<NodeID, std::vector<NodeID>> key);
    void removeNack(std::tuple<NodeID, std::vector<NodeID>> key);
    void addCommit(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender);
    void removeCommit(std::tuple<NodeID, std::vector<NodeID>> key);
    void addCheckAck(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender);
    void addCheckNack(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender);
    void removeCheckAck(std::tuple<NodeID, std::vector<NodeID>> key);
    void removeCheckNack(std::tuple<NodeID, std::vector<NodeID>> key);

    SCPQuorumSet const& getQuorumSet();
    std::set<NodeID> getTombSet();
    std::set<std::tuple<NodeID, std::vector<NodeID>>> getTentative();
    std::vector<std::vector<NodeID>> getTentativeSet();
    std::set<NodeID> getAck(std::tuple<NodeID, std::vector<NodeID>> key);
    std::set<NodeID> getNack(std::tuple<NodeID, std::vector<NodeID>> key);
    std::set<NodeID> getCommit(std::tuple<NodeID, std::vector<NodeID>> key);
    std::set<NodeID> getCheckAck(std::tuple<NodeID, std::vector<NodeID>> key);
    std::set<NodeID> getCheckNack(std::tuple<NodeID, std::vector<NodeID>> key);
    bool isAckNackComplete(std::tuple<NodeID, std::vector<NodeID>> key);
    Hash const& getQuorumSetHash();
    bool isValidator();
    void addNewQuorum(std::vector<NodeID> newQ);

    // returns the quorum set {{X}}
    static SCPQuorumSetPtr getSingletonQSet(NodeID const& nodeID);

    // runs proc over all nodes contained in qset, but fast fails if proc fails
    static bool forAllNodes(SCPQuorumSet const& qset,
                            std::function<bool(NodeID const&)> proc);

    // returns the weight of the node within the qset
    // normalized between 0-UINT64_MAX
    static uint64 getNodeWeight(NodeID const& nodeID, SCPQuorumSet const& qset);

    // Tests this node against nodeSet for the specified qSethash.
    static bool isQuorumSlice(SCPQuorumSet const& qSet,
                              std::vector<NodeID> const& nodeSet);
    static bool isVBlocking(SCPQuorumSet const& qSet,
                            std::vector<NodeID> const& nodeSet);

    // intersection test for leave request
    static bool isQuorumPure(NodeID const& checkedNode, stellar::QuorumTracker::QuorumMap const& qMap,
                                 std::vector<NodeID> const& nodeSet);
    
    static std::vector<std::vector<NodeID>> computeSortedPowerSet(std::vector<NodeID> const& allValidators, int n);

    static std::vector<std::vector<NodeID>> findMinQuorum(NodeID const& checkedNode, 
                                                            //std::vector<NodeID> const& allValidators, 
                                                            stellar::QuorumTracker::QuorumMap const& qMap);
    static bool isQuorumBlocking(std::vector<std::vector<NodeID>> const& minQs,
                                 std::vector<NodeID> const& nodeSet);
    static bool isQuorumInclusion(std::vector<std::vector<NodeID>> const& minQs,
                                 std::vector<NodeID> const& nodeSet);
    static bool isSubset(const std::vector<NodeID>& vectorA, const std::vector<NodeID>& vectorB);

    static bool leaveCheck(std::vector<std::vector<NodeID>> const& minQs,
                                 std::set<NodeID> const& tomb, NodeID const& leavingNode);
    static bool addCheck(std::vector<std::vector<NodeID>> const& minQs,
                                 std::vector<std::vector<NodeID>> const& tentative, std::vector<NodeID> const& q_c);

    // returns the union of all quorums
    static std::set<NodeID> getQuorumUnion(NodeID const& nodeID, stellar::QuorumTracker::QuorumMap const& qMap);

    // Tests this node against a map of nodeID -> T for the specified qSetHash.

    // `isVBlocking` tests if the filtered nodes V are a v-blocking set for
    // this node.
    static bool isVBlocking(
        SCPQuorumSet const& qSet,
        std::map<NodeID, SCPEnvelopeWrapperPtr> const& map,
        std::function<bool(SCPStatement const&)> const& filter =
            [](SCPStatement const&) { return true; });

    // `isQuorum` tests if the filtered nodes V form a quorum
    // (meaning for each v \in V there is q \in Q(v)
    // included in V and we have quorum on V for qSetHash). `qfun` extracts the
    // SCPQuorumSetPtr from the SCPStatement for its associated node in map
    // (required for transitivity)
    static bool isQuorum(
        SCPQuorumSet const& qSet,
        std::map<NodeID, SCPEnvelopeWrapperPtr> const& map,
        std::function<SCPQuorumSetPtr(SCPStatement const&)> const& qfun,
        std::function<bool(SCPStatement const&)> const& filter =
            [](SCPStatement const&) { return true; });

    // computes the distance to the set of v-blocking sets given
    // a set of nodes that agree (but can fail)
    // excluded, if set will be skipped altogether
    static std::vector<NodeID>
    findClosestVBlocking(SCPQuorumSet const& qset,
                         std::set<NodeID> const& nodes, NodeID const* excluded);

    static std::vector<NodeID> findClosestVBlocking(
        SCPQuorumSet const& qset,
        std::map<NodeID, SCPEnvelopeWrapperPtr> const& map,
        std::function<bool(SCPStatement const&)> const& filter =
            [](SCPStatement const&) { return true; },
        NodeID const* excluded = nullptr);

    static Json::Value toJson(SCPQuorumSet const& qSet,
                              std::function<std::string(NodeID const&)> r);

    Json::Value toJson(SCPQuorumSet const& qSet, bool fullKeys) const;
    std::string to_string(SCPQuorumSet const& qSet) const;

    static uint64 computeWeight(uint64 m, uint64 total, uint64 threshold);

  protected:
    // returns a quorum set {{ nodeID }}
    static SCPQuorumSet buildSingletonQSet(NodeID const& nodeID);

    // called recursively
    static bool isQuorumSliceInternal(SCPQuorumSet const& qset,
                                      std::vector<NodeID> const& nodeSet);
    static bool isVBlockingInternal(SCPQuorumSet const& qset,
                                    std::vector<NodeID> const& nodeSet);
};
}
