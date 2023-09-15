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
#include "scp/SCP.h"
#include <set>

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
    //the current tracked quorums slices are app.getHerder().getCurrentlyTrackedQuorum()

    //the first step is to list all the minimal quorums of current node
    //std::vector<std::vector<NodeID>> minQs = stellar::LocalNode::findMinQuorum(getSourceID(), mLeave.qSet)
    // perform leave check based on quorums passed in: need to extract the quorums
    std::vector<std::vector<NodeID>> minQs;
    for(auto it : mLeave.qSet.innerSets){
        minQs.emplace_back(it.validators);
    }

    std::vector<NodeID> emptyTomb;
    bool leaveResult = stellar::LocalNode::leaveCheck(minQs, emptyTomb, mLeave.destination);
    if(leaveResult){
        innerResult().code(LEAVE_SUCCESS);
    }
    else{
        innerResult().code(LEAVE_MALFORMED);
    }
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
