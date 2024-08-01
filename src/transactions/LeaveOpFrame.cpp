
// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "util/asio.h"
#include "transactions/LeaveOpFrame.h"
#include "OfferExchange.h"
#include "database/Database.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "ledger/LedgerTxnHeader.h"
#include "transactions/SponsorshipUtils.h"
#include "transactions/TransactionUtils.h"
#include "util/GlobalChecks.h"
#include "util/Logging.h"
#include "util/ProtocolVersion.h"
#include "util/XDROperators.h"
#include <Tracy.hpp>
#include <algorithm>

#include "scp/QuorumSetUtils.h"
#include "scp/LocalNode.h"
#include "util/XDROperators.h"
#include "xdr/Stellar-SCP.h"
#include "xdr/Stellar-types.h"
#include "herder/QuorumTracker.h"
#include "herder/Herder.h"
#include "herder/HerderImpl.h"
#include "scp/SCP.h"
#include <set>
#include "util/Logging.h"
#include "main/Application.h"

namespace stellar
{

using namespace std;

LeaveOpFrame::LeaveOpFrame(Operation const& op,
                                           OperationResult& res,
                                           TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mLeave(mOperation.body.leaveOp())
{
}

bool LeaveOpFrame::doApply(AbstractLedgerTxn& ltx){
    ZoneNamedN(applyZone, "LeaveOp apply", true);
    return true;
}

bool
LeaveOpFrame::doApply(Application& app, AbstractLedgerTxn& ltx,
                        Hash const& sorobanBasePrngSeed)
{
    ZoneNamedN(applyZone, "LeaveOp apply", true);
    //We do not modify the ledger for leave operation
    //We need to update quorums, which is stored in app
    //destination is the leaving nodes's NodeID, qSet is the quorum system passed in 
    //We use qSet.innnerSets.validators to store one minimal quorum 
    //the current tracked quorums slices are app.getHerder().getCurrentlyTrackedQuorum()

    //the first step is to list all the minimal quorums from qSet
    //std::vector<std::vector<NodeID>> minQs = stellar::LocalNode::findMinQuorum(getSourceID(), mLeave.qSet)
    
    // we already passsed in minimal quorums
    std::vector<std::vector<NodeID>> minQs;
    for(auto it : mLeave.qSet.innerSets){
        minQs.emplace_back(it.validators);
    }
    // perform leave check based on extracted quorums from qSet
    //std::vector<NodeID> emptyTomb;
    std::set<NodeID> tomb = static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNode()->getTombSet();
    bool leaveResult = stellar::LocalNode::leaveCheck(minQs, tomb, mLeave.destination);
    if(leaveResult){
        //If the leave request is approved, remove mLeave.destination from current node's quorum slices
        //double check whether the local quorum can be updated with the constant key word
        for(auto it : app.getHerder().getCurrentlyTrackedQuorum()){
            if(it.first == static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNodeID()){
                SCPQuorumSet updatedQ;
                updatedQ = stellar::removeNodeQSet(static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalQuorumSet(), mLeave.destination, app.getHerder().getCurrentlyTrackedQuorum());
                //update local quorum set
                static_cast<HerderImpl&>(app.getHerder()).getSCP().updateLocalQuorumSet(updatedQ);
                static_cast<HerderImpl&>(app.getHerder()).updateQMap(it.first, updatedQ);
            }
            else{
                //remove leave node from other tracked quorum's slices so that after removal the leave node is not present in the local minimal quorum
                SCPQuorumSet updatedQ = stellar::removeNodeQSet(*(it.second.mQuorumSet), mLeave.destination, app.getHerder().getCurrentlyTrackedQuorum());
                //update quorum set for nodes in quorum map
                static_cast<HerderImpl&>(app.getHerder()).updateQMap(it.first, updatedQ);
                //static_cast<HerderImpl&>(app.getHerder()).getSCP().updateLocalQuorumSet(updatedQ);
            }
        }
        SCPQuorumSet emptyQ;
        static_cast<HerderImpl&>(app.getHerder()).updateQMap(mLeave.destination, emptyQ);
        //SCPQuorumSet updatedQ;
        //updatedQ = stellar::removeNodeQSet(static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalQuorumSet(), mLeave.destination, app.getHerder().getCurrentlyTrackedQuorum());
        //update local quorum set
        //static_cast<HerderImpl&>(app.getHerder()).getSCP().updateLocalQuorumSet(updatedQ);

        //add the left node in the tombset
        tomb.emplace(mLeave.destination);
        static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNode()->updateTombSet(tomb);

        //For the leaving node itself, it should stop
        if (mLeave.destination == static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNodeID()){
            app.gracefulStop();
        }
        innerResult().code(LEAVE_SUCCESS);
    }
    else{
        innerResult().code(LEAVE_MALFORMED);
    }

    // record leave transaction completion time
    // Get the current time as a time_point object
    auto now = std::chrono::system_clock::now();
    // Convert the time_point to a duration since epoch
    auto duration = now.time_since_epoch();
    // Convert the duration to milliseconds
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    CLOG_INFO(Tx, "Transactions: leave transaction complete at {}", millis);

    return true;
}

bool
LeaveOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    // we do not need validity check at this time.
    //potentially, we can check whether qSet is the quorum system of destination
    //if (mLeave.destination == getSourceID())
    //{
        //innerResult().code(LEAVE_MALFORMED);
        //return false;
    //}

    return true;
}

void
LeaveOpFrame::insertLedgerKeysToPrefetch(
    UnorderedSet<LedgerKey>& keys) const
{
    //For leave operation, we do not need to modify the database
    //keys.emplace(accountKey(mCreateAccount.destination));
}
}
