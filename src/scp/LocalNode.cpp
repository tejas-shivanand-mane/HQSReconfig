// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "LocalNode.h"

#include "crypto/Hex.h"
#include "crypto/SecretKey.h"
#include "lib/json/json.h"
#include "scp/QuorumSetUtils.h"
#include "util/GlobalChecks.h"
#include "util/Logging.h"
#include "util/XDROperators.h"
#include "util/numeric.h"
#include "xdrpp/marshal.h"
#include <Tracy.hpp>
#include <algorithm>
#include <functional>
#include "herder/QuorumTracker.h"
#include "scp/SCP.h"

namespace stellar
{
LocalNode::LocalNode(NodeID const& nodeID, bool isValidator,
                     SCPQuorumSet const& qSet, SCPDriver& driver)
    : mNodeID(nodeID), mIsValidator(isValidator), mQSet(qSet), mDriver(driver)
{
    normalizeQSet(mQSet);
    mQSetHash = driver.getHashOf({xdr::xdr_to_opaque(mQSet)});

    CLOG_INFO(SCP, "LocalNode::LocalNode@{} qSet: {}",
              driver.toShortString(mNodeID), hexAbbrev(mQSetHash));

    mSingleQSet = std::make_shared<SCPQuorumSet>(buildSingletonQSet(mNodeID));
    gSingleQSetHash = driver.getHashOf({xdr::xdr_to_opaque(*mSingleQSet)});
}

SCPQuorumSet
LocalNode::buildSingletonQSet(NodeID const& nodeID)
{
    SCPQuorumSet qSet;
    qSet.threshold = 1;
    qSet.validators.emplace_back(nodeID);
    return qSet;
}

void
LocalNode::updateQuorumSet(SCPQuorumSet const& qSet)
{
    ZoneScoped;
    mQSetHash = mDriver.getHashOf({xdr::xdr_to_opaque(qSet)});
    mQSet = qSet;
}

void
LocalNode::updateTombSet(std::set<NodeID> tSet)
{
    mTombSet = tSet;
}

void
LocalNode::addTentative(std::tuple<NodeID, std::vector<NodeID>> addT)
{
    mTentative.insert(addT);
}

void
LocalNode::removeTentative(std::tuple<NodeID, std::vector<NodeID>> removeT)
{
    mTentative.erase(removeT);
}

void 
LocalNode::addAck(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender)
{
    mAck[key].insert(sender);

}
void 
LocalNode::addNack(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender)
{
    mNack[key].insert(sender);
}
void 
LocalNode::removeAck(std::tuple<NodeID, std::vector<NodeID>> key)
{
    mAck.erase(key);
}
void 
LocalNode::removeNack(std::tuple<NodeID, std::vector<NodeID>> key)
{
    mNack.erase(key);
}

void 
LocalNode::addCommit(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender)
{
    mCommit[key].insert(sender);

}

void 
LocalNode::removeCommit(std::tuple<NodeID, std::vector<NodeID>> key)
{
    mCommit.erase(key);
}

void 
LocalNode::addCheckAck(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender)
{
    mCheckAck[key].insert(sender);

}
void 
LocalNode::addCheckNack(std::tuple<NodeID, std::vector<NodeID>> key, NodeID sender)
{
    mCheckNack[key].insert(sender);
}
void 
LocalNode::removeCheckAck(std::tuple<NodeID, std::vector<NodeID>> key)
{
    mCheckAck.erase(key);
}
void 
LocalNode::removeCheckNack(std::tuple<NodeID, std::vector<NodeID>> key)
{
    mCheckNack.erase(key);
}

void 
LocalNode::addNewQuorum(std::vector<NodeID> newQ)
{
    
}

SCPQuorumSet const&
LocalNode::getQuorumSet()
{
    return mQSet;
}

std::set<NodeID>
LocalNode::getTombSet()
{
    return mTombSet;
}

std::set<std::tuple<NodeID, std::vector<NodeID>>>
LocalNode::getTentative()
{
    return mTentative;
}

std::vector<std::vector<NodeID>>
LocalNode::getTentativeSet()
{
    std::vector<std::vector<NodeID>> tentativeSet;
    for(auto it : mTentative){
        tentativeSet.emplace_back(std::get<1>(it));
    }
    return tentativeSet;
}

std::set<NodeID> 
LocalNode::getAck(std::tuple<NodeID, std::vector<NodeID>> key)
{
    return mAck[key];
}

std::set<NodeID> 
LocalNode::getNack(std::tuple<NodeID, std::vector<NodeID>> key)
{
    return mNack[key];
}

std::set<NodeID> 
LocalNode::getCommit(std::tuple<NodeID, std::vector<NodeID>> key)
{
    return mCommit[key];
}

std::set<NodeID> 
LocalNode::getCheckAck(std::tuple<NodeID, std::vector<NodeID>> key)
{
    return mCheckAck[key];
}

std::set<NodeID> 
LocalNode::getCheckNack(std::tuple<NodeID, std::vector<NodeID>> key)
{
    return mCheckNack[key];
}

bool 
LocalNode::isAckNackComplete(std::tuple<NodeID, std::vector<NodeID>> key)
{
    // all the nodes in q_c have acked or nacked
    for(auto it = std::get<1>(key).begin(); it != std::get<1>(key).end(); ++it){
        bool completed = false;
        if(mAck[key].find(*it) != mAck[key].end()){
            completed = true;
        }
        if(mNack[key].find(*it) != mNack[key].end()){
            completed = true;
        }
        if (!completed){
            return false;
        }
    }
    return true;
}

Hash const&
LocalNode::getQuorumSetHash()
{
    return mQSetHash;
}

SCPQuorumSetPtr
LocalNode::getSingletonQSet(NodeID const& nodeID)
{
    return std::make_shared<SCPQuorumSet>(buildSingletonQSet(nodeID));
}

bool
LocalNode::forAllNodes(SCPQuorumSet const& qset,
                       std::function<bool(NodeID const&)> proc)
{
    for (auto const& n : qset.validators)
    {
        if (!proc(n))
        {
            return false;
        }
    }
    for (auto const& q : qset.innerSets)
    {
        if (!forAllNodes(q, proc))
        {
            return false;
        }
    }
    return true;
}

uint64
LocalNode::computeWeight(uint64 m, uint64 total, uint64 threshold)
{
    uint64 res;
    releaseAssert(threshold <= total);
    // Since threshold <= total, calculating res=m*threshold/total will always
    // produce res <= m, and we do not need to handle the possibility of this
    // call returning false (indicating overflow).
    bool noOverflow = bigDivideUnsigned(res, m, threshold, total, ROUND_UP);
    releaseAssert(noOverflow);
    return res;
}

// if a validator is repeated multiple times its weight is only the
// weight of the first occurrence
uint64
LocalNode::getNodeWeight(NodeID const& nodeID, SCPQuorumSet const& qset)
{
    uint64 n = qset.threshold;
    uint64 d = qset.innerSets.size() + qset.validators.size();
    uint64 res;

    for (auto const& qsetNode : qset.validators)
    {
        if (qsetNode == nodeID)
        {
            res = computeWeight(UINT64_MAX, d, n);
            return res;
        }
    }

    for (auto const& q : qset.innerSets)
    {
        uint64 leafW = getNodeWeight(nodeID, q);
        if (leafW)
        {
            res = computeWeight(leafW, d, n);
            return res;
        }
    }

    return 0;
}

bool
LocalNode::isQuorumSliceInternal(SCPQuorumSet const& qset,
                                 std::vector<NodeID> const& nodeSet)
{
    uint32 thresholdLeft = qset.threshold;
    for (auto const& validator : qset.validators)
    {
        auto it = std::find(nodeSet.begin(), nodeSet.end(), validator);
        if (it != nodeSet.end())
        {
            thresholdLeft--;
            if (thresholdLeft <= 0)
            {
                return true;
            }
        }
    }

    for (auto const& inner : qset.innerSets)
    {
        if (isQuorumSliceInternal(inner, nodeSet))
        {
            thresholdLeft--;
            if (thresholdLeft <= 0)
            {
                return true;
            }
        }
    }
    return false;
}

bool
LocalNode::isQuorumSlice(SCPQuorumSet const& qSet,
                         std::vector<NodeID> const& nodeSet)
{
    return isQuorumSliceInternal(qSet, nodeSet);
}

// called recursively
bool
LocalNode::isVBlockingInternal(SCPQuorumSet const& qset,
                               std::vector<NodeID> const& nodeSet)
{
    // There is no v-blocking set for {\empty}
    if (qset.threshold == 0)
    {
        return false;
    }

    int leftTillBlock =
        (int)((1 + qset.validators.size() + qset.innerSets.size()) -
              qset.threshold);

    for (auto const& validator : qset.validators)
    {
        auto it = std::find(nodeSet.begin(), nodeSet.end(), validator);
        if (it != nodeSet.end())
        {
            leftTillBlock--;
            if (leftTillBlock <= 0)
            {
                return true;
            }
        }
    }
    for (auto const& inner : qset.innerSets)
    {
        if (isVBlockingInternal(inner, nodeSet))
        {
            leftTillBlock--;
            if (leftTillBlock <= 0)
            {
                return true;
            }
        }
    }

    return false;
}

bool
LocalNode::isVBlocking(SCPQuorumSet const& qSet,
                       std::vector<NodeID> const& nodeSet)
{
    return isVBlockingInternal(qSet, nodeSet);
}

bool
LocalNode::isVBlocking(SCPQuorumSet const& qSet,
                       std::map<NodeID, SCPEnvelopeWrapperPtr> const& map,
                       std::function<bool(SCPStatement const&)> const& filter)
{
    ZoneScoped;
    std::vector<NodeID> pNodes;
    for (auto const& it : map)
    {
        if (filter(it.second->getStatement()))
        {
            pNodes.push_back(it.first);
        }
    }

    return isVBlocking(qSet, pNodes);
}

bool
LocalNode::isQuorum(
    SCPQuorumSet const& qSet,
    std::map<NodeID, SCPEnvelopeWrapperPtr> const& map,
    std::function<SCPQuorumSetPtr(SCPStatement const&)> const& qfun,
    std::function<bool(SCPStatement const&)> const& filter)
{
    ZoneScoped;
    std::vector<NodeID> pNodes;
    for (auto const& it : map)
    {
        if (filter(it.second->getStatement()))
        {
            pNodes.push_back(it.first);
        }
    }

    size_t count = 0;
    do
    {
        count = pNodes.size();
        std::vector<NodeID> fNodes(pNodes.size());
        auto quorumFilter = [&](NodeID nodeID) -> bool {
            auto qSetPtr = qfun(map.find(nodeID)->second->getStatement());
            if (qSetPtr)
            {
                return isQuorumSlice(*qSetPtr, pNodes);
            }
            else
            {
                return false;
            }
        };
        auto it = std::copy_if(pNodes.begin(), pNodes.end(), fNodes.begin(),
                               quorumFilter);
        fNodes.resize(std::distance(fNodes.begin(), it));
        pNodes = fNodes;
    } while (count != pNodes.size());

    return isQuorumSlice(qSet, pNodes);
}

//This is function that verifies whether a nodeSet is a transitive quorum
bool
LocalNode::isQuorumPure(NodeID const& checkedNode, stellar::QuorumTracker::QuorumMap const& qMap,
                                 std::vector<NodeID> const& nodeSet)
{
    //for each node in the nodeSet, verify whether nodeSet contains a quorum slice
    for (auto const& validator : nodeSet)
    {
        auto it = qMap.find(validator);
        if(it != qMap.end()){
            if(!isQuorumSlice(*(it->second.mQuorumSet), nodeSet)){
                return false;
            }
        }
        // the validator is not known
        else{
            return false;
        }
    }

    // verify the current node has a quorum slice in this quorum: this is a quorum for checkedNode
    auto it = qMap.find(checkedNode);
        if(it != qMap.end()){
            // nodeSet is not a quorum for checkedNode
            if(!isQuorumSlice(*(it->second.mQuorumSet), nodeSet)){
                return false;
            }
        }
        else{
            return false;
        }
    return true;
}

std::vector<std::vector<NodeID>>
LocalNode::computeSortedPowerSet(std::vector<NodeID> const& allValidators, int n) {
    std::vector<std::vector<NodeID>> powerSet;

    for (int counter = 0; counter < (1 << n); counter++) {
        std::vector<NodeID> subset;
        for (int j = 0; j < n; j++) {
            if (counter & (1 << j)) {
                subset.push_back(allValidators[j]);
            }
        }
        powerSet.push_back(subset);
    }

     // Sort subsets by cardinality (size)
    sort(powerSet.begin(), powerSet.end(), [](const std::vector<NodeID>& a, const std::vector<NodeID>& b) {
        return a.size() < b.size();
    });

    //std::vector<std::vector<NodeID>> sortedPowerSet;
    // Print the sorted power set
    //for (const std::vector<NodeID>& subset : powerSet) {
        //cout << "{ ";
        //for (char ch : subset) {
            //cout << ch << " ";
        //}
        //cout << "}" << endl;
    //}
    return powerSet;
}

// when we list minimal quorums, we need to confirm it is a minimal quorum for the checkedNode
std::vector<std::vector<NodeID>>
LocalNode::findMinQuorum(NodeID const& checkedNode, stellar::QuorumTracker::QuorumMap const& qMap) {
//LocalNode::findMinQuorum(NodeID const& checkedNode, std::vector<NodeID> const& allValidators, stellar::QuorumTracker::QuorumMap const& qMap) {
    std::vector<NodeID> allValidators;
    for (const auto& pair : qMap) {
        allValidators.emplace_back(pair.first);
    }
    std::vector<std::vector<NodeID>> minQ;
    //list the powerset by cardinality order
    if(allValidators.size() != 0){
        auto sortedPowerset = computeSortedPowerSet(allValidators, allValidators.size());
        for(size_t i = 0; i < sortedPowerset.size(); i++){
            std::vector<NodeID> subset = sortedPowerset[i];
            if(subset.size() == 0){
                continue;
            }
            bool supersetForMinQ = false;
            for(auto q : minQ){
                // The current considered set is a superset of some minimal quorum. Therefore, it cannot be a minimal quorum
                for (const auto& e : q) {
                    if (std::find(subset.begin(), subset.end(), e) == subset.end()) {
                        supersetForMinQ = false;
                        break; 
                    }
                    supersetForMinQ = true;
                }
                if(supersetForMinQ){
                    break;
                }

                //if(std::includes(subset.begin(), subset.end(), q.begin(), q.end())){
                    //supersetForMinQ = true;
                    //break;
                //}
            }
            // the current considered set is not a super set of any minimal quorum and is a quorum. 
            // Since any strict subset of the current considered set is not a quorum, the current set is a minimal quorum.
            if(!supersetForMinQ && isQuorumPure(checkedNode, qMap, subset)){
                minQ.emplace_back(subset);
            }
        }
        return minQ;
    }
    else{
        //std::vector<std::vector<NodeID>> emptySet;
        //return emptySet;
        return minQ;
    }
}

std::set<NodeID> 
LocalNode::getQuorumUnion(NodeID const& nodeID, stellar::QuorumTracker::QuorumMap const& qMap) {
    std::set<NodeID> quorumUnion;
    for (auto minQs: LocalNode::findMinQuorum(nodeID, qMap)) {
        for (auto p: minQs) {
            quorumUnion.insert(p);
        }
    }
    return quorumUnion;
}

// check whether a nodeSet is quorum blocking
bool
LocalNode::isQuorumBlocking(std::vector<std::vector<NodeID>> const& minQs,
                                 std::vector<NodeID> const& nodeSet)
{
    //bool existQNotBlocked = false;
    for(auto q : minQs){
        std::vector<NodeID> intersection;
        std::set_intersection(q.begin(), q.end(), nodeSet.begin(), nodeSet.end(),
                          std::back_inserter(intersection));
        if(intersection.empty()){
            return false;
        }
    }
    return true;
}

// inclusion check: check whether a quorum in minQs is a subset of nodeSet
bool
LocalNode::isQuorumInclusion(std::vector<std::vector<NodeID>> const& minQs,
                                 std::vector<NodeID> const& nodeSet)
{
    for(auto q : minQs){
        // if there exist q \in minQs such that q is a subset of nodeSet
        bool isSubset = std::includes(nodeSet.begin(), nodeSet.end(), q.begin(), q.end());
        if(isSubset){
            return true;
        }
    }
    return false;
}

//check whether a quorum system can let tombset and p leave
//This is the intersection check performed in our paper for leave.
bool
LocalNode::leaveCheck(std::vector<std::vector<NodeID>> const& minQs,
                                 std::set<NodeID> const& tomb, NodeID const& leavingNode)
{
    
    for(size_t i = 0; i < minQs.size(); ++i){
        for(size_t j = i + 1; j < minQs.size(); ++j){
            std::vector<NodeID> intersection;
            std::set_intersection(minQs[i].begin(), minQs[i].end(), minQs[j].begin(), minQs[j].end(),
                          std::back_inserter(intersection));
            for (auto element : tomb) {
                auto it = std::find(intersection.begin(), intersection.end(), element);
                if (it != intersection.end()) {
                    intersection.erase(it);
                }
            }
            auto it = std::find(intersection.begin(), intersection.end(), leavingNode);
                if (it != intersection.end()) {
                    intersection.erase(it);
                }
            if(!isQuorumBlocking(minQs, intersection)){
                return false;
            }
        }
    }
    return true;
}

//check whether a quorum system can add qc considering the current tentative.
//This is the intersection check performed in our paper for add.
bool
LocalNode::addCheck(std::vector<std::vector<NodeID>> const& minQs,
                                 std::vector<std::vector<NodeID>> const& tentative, std::vector<NodeID> const& q_c)
{
    // first test the intersections between quorums in minQs and q_c
    for(size_t i = 0; i < minQs.size(); ++i){
        std::vector<NodeID> intersection;
        std::set_intersection(minQs[i].begin(), minQs[i].end(), q_c.begin(), q_c.end(),
                        std::back_inserter(intersection));
        //TODO: this is for later when we combine leave and add
        //for (auto element : tomb) {
        //    auto it = std::find(intersection.begin(), intersection.end(), element);
        //    if (it != intersection.end()) {
        //        intersection.erase(it);
        //    }
        //}
        //auto it = std::find(intersection.begin(), intersection.end(), leavingNode);
        //    if (it != intersection.end()) {
        //        intersection.erase(it);
        //    }

        if(!isQuorumBlocking(minQs, intersection)){
            return false;
        }
    }
    // Then test the intersections between tentative and q_c
    for(size_t i = 0; i < tentative.size(); ++i){
        std::vector<NodeID> intersection;
        std::set_intersection(tentative[i].begin(), tentative[i].end(), q_c.begin(), q_c.end(),
                        std::back_inserter(intersection));
        //TODO: this is for later when we combine leave and add
        //for (auto element : tomb) {
        //    auto it = std::find(intersection.begin(), intersection.end(), element);
        //    if (it != intersection.end()) {
        //        intersection.erase(it);
        //    }
        //}
        //auto it = std::find(intersection.begin(), intersection.end(), leavingNode);
        //    if (it != intersection.end()) {
        //        intersection.erase(it);
        //    }

        if(!isQuorumBlocking(minQs, intersection)){
            return false;
        }
    }
    return true;
}

std::vector<NodeID>
LocalNode::findClosestVBlocking(
    SCPQuorumSet const& qset,
    std::map<NodeID, SCPEnvelopeWrapperPtr> const& map,
    std::function<bool(SCPStatement const&)> const& filter,
    NodeID const* excluded)
{
    std::set<NodeID> s;
    for (auto const& n : map)
    {
        if (filter(n.second->getStatement()))
        {
            s.emplace(n.first);
        }
    }
    return findClosestVBlocking(qset, s, excluded);
}

std::vector<NodeID>
LocalNode::findClosestVBlocking(SCPQuorumSet const& qset,
                                std::set<NodeID> const& nodes,
                                NodeID const* excluded)
{
    ZoneScoped;
    size_t leftTillBlock =
        ((1 + qset.validators.size() + qset.innerSets.size()) - qset.threshold);

    std::vector<NodeID> res;

    // first, compute how many top level items need to be blocked
    for (auto const& validator : qset.validators)
    {
        if (!excluded || !(validator == *excluded))
        {
            auto it = nodes.find(validator);
            if (it == nodes.end())
            {
                leftTillBlock--;
                if (leftTillBlock == 0)
                {
                    // already blocked
                    return std::vector<NodeID>();
                }
            }
            else
            {
                // save this for later
                res.emplace_back(validator);
            }
        }
    }

    struct orderBySize
    {
        bool
        operator()(std::vector<NodeID> const& v1,
                   std::vector<NodeID> const& v2) const
        {
            return v1.size() < v2.size();
        }
    };

    std::multiset<std::vector<NodeID>, orderBySize> resInternals;

    for (auto const& inner : qset.innerSets)
    {
        auto v = findClosestVBlocking(inner, nodes, excluded);
        if (v.size() == 0)
        {
            leftTillBlock--;
            if (leftTillBlock == 0)
            {
                // already blocked
                return std::vector<NodeID>();
            }
        }
        else
        {
            resInternals.emplace(v);
        }
    }

    // use the top level validators to get closer
    if (res.size() > leftTillBlock)
    {
        res.resize(leftTillBlock);
    }
    leftTillBlock -= res.size();

    // use subsets to get closer, using the smallest ones first
    auto it = resInternals.begin();
    while (leftTillBlock != 0 && it != resInternals.end())
    {
        res.insert(res.end(), it->begin(), it->end());
        leftTillBlock--;
        it++;
    }

    return res;
}

Json::Value
LocalNode::toJson(SCPQuorumSet const& qSet, bool fullKeys) const
{
    return toJson(
        qSet, [&](NodeID const& k) { return mDriver.toStrKey(k, fullKeys); });
}

Json::Value
LocalNode::toJson(SCPQuorumSet const& qSet,
                  std::function<std::string(NodeID const&)> r)
{
    Json::Value ret;
    ret["t"] = qSet.threshold;
    auto& entries = ret["v"];
    for (auto const& v : qSet.validators)
    {
        entries.append(r(v));
    }
    for (auto const& s : qSet.innerSets)
    {
        entries.append(toJson(s, r));
    }
    return ret;
}

std::string
LocalNode::to_string(SCPQuorumSet const& qSet) const
{
    Json::FastWriter fw;
    return fw.write(toJson(qSet, false));
}

NodeID const&
LocalNode::getNodeID()
{
    return mNodeID;
}

bool
LocalNode::isValidator()
{
    return mIsValidator;
}
}
