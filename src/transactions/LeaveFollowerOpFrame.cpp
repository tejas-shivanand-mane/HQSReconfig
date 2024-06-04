
// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "util/asio.h"
#include "transactions/LeaveFollowerOpFrame.h"
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

LeaveFollowerOpFrame::LeaveFollowerOpFrame(Operation const& op,
                                           OperationResult& res,
                                           TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mLeaveFollower(mOperation.body.leaveFollowerOp())
{
}

bool LeaveFollowerOpFrame::doApply(AbstractLedgerTxn& ltx){
    ZoneNamedN(applyZone, "LeaveFollowerOp apply", true);
    return true;
}

bool
LeaveFollowerOpFrame::doApply(Application& app, AbstractLedgerTxn& ltx,
                        Hash const& sorobanBasePrngSeed)
{
    ZoneNamedN(applyZone, "LeaveFollowerOp apply", true);
    //We do not modify the ledger for leave follower operation
    //We need to update quorums, which is stored in app
    //destination is the leaving nodes's NodeID
    //the current tracked quorums slices are app.getHerder().getCurrentlyTrackedQuorum()

    for(auto it : app.getHerder().getCurrentlyTrackedQuorum()){
        if(it.first == static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNodeID()){
            SCPQuorumSet updatedQ;
            updatedQ = stellar::removeNodeQSet(static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalQuorumSet(), mLeaveFollower.destination, app.getHerder().getCurrentlyTrackedQuorum());
            //update local quorum set
            static_cast<HerderImpl&>(app.getHerder()).getSCP().updateLocalQuorumSet(updatedQ);
            static_cast<HerderImpl&>(app.getHerder()).updateQMap(it.first, updatedQ);
        }
        else{
            SCPQuorumSet updatedQ = stellar::removeNodeQSet(*(it.second.mQuorumSet), mLeaveFollower.destination, app.getHerder().getCurrentlyTrackedQuorum());
            //update quorum set for nodes in quorum map
            static_cast<HerderImpl&>(app.getHerder()).updateQMap(it.first, updatedQ);
            //static_cast<HerderImpl&>(app.getHerder()).getSCP().updateLocalQuorumSet(updatedQ);
        }
        
        SCPQuorumSet emptyQ;
        static_cast<HerderImpl&>(app.getHerder()).updateQMap(mLeaveFollower.destination, emptyQ);
        //SCPQuorumSet updatedQ;
        //updatedQ = stellar::removeNodeQSet(static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalQuorumSet(), mLeave.destination, app.getHerder().getCurrentlyTrackedQuorum());
        //update local quorum set
        //static_cast<HerderImpl&>(app.getHerder()).getSCP().updateLocalQuorumSet(updatedQ);

        //add the left node in the tombset
        std::set<NodeID> tomb = static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNode()->getTombSet();
        tomb.emplace(mLeaveFollower.destination);
        static_cast<HerderImpl&>(app.getHerder()).getSCP().getLocalNode()->updateTombSet(tomb);

        innerResult().code(LEAVE_FOLLOWER_SUCCESS);
    }
    //innerResult().code(LEAVE_FOLLOWER_MALFORMED);
    return true;
}

bool
LeaveFollowerOpFrame::doCheckValid(uint32_t ledgerVersion)
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
LeaveFollowerOpFrame::insertLedgerKeysToPrefetch(
    UnorderedSet<LedgerKey>& keys) const
{
    //For leave follower operation, we do not need to modify the database
    //keys.emplace(accountKey(mCreateAccount.destination));
}
}
