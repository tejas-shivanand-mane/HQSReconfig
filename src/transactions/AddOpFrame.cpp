
// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "util/asio.h"
#include "transactions/AddOpFrame.h"
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
#include "overlay/Peer.h"
#include "overlay/OverlayManager.h"
#include "overlay/OverlayManagerImpl.h"

namespace stellar
{

using namespace std;

AddOpFrame::AddOpFrame(Operation const& op,
                                           OperationResult& res,
                                           TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mAdd(mOperation.body.addOp())
{
}

bool AddOpFrame::doApply(AbstractLedgerTxn& ltx){
    ZoneNamedN(applyZone, "AddOp apply", true);
    return true;
}

bool
AddOpFrame::doApply(Application& app, AbstractLedgerTxn& ltx,
                        Hash const& sorobanBasePrngSeed)
{
    ZoneNamedN(applyZone, "AddOp apply", true);
    //We do not modify the ledger for add operation
    //We need to update quorums, which is stored in app
    //destination is the requesting nodes's NodeID, qSet's validator field stores the new quorum 
    //the current tracked quorums slices are app.getHerder().getCurrentlyTrackedQuorum()
    
    std::vector<NodeID> newQ;
    bool inNewQ = false;
    auto& herder = static_cast<HerderImpl&>(app.getHerder());
    auto localNodeID = herder.getSCP().getLocalNodeID();
    for(auto it : mAdd.qSet.validators){
        newQ.emplace_back(it);
        if (it == localNodeID) {
            inNewQ = true;
        }
    }
    if (inNewQ) {
        // for the requesting node, initialize entries in ack and nack maps
        //if mAdd.destination == localNodeID {
        //    herder.getLocalNode().addTentative(std::make_tuple(localNodeID, newQ));
        //}
        //If the new quorum contains the local node, do inclusion check
        std::vector<std::vector<NodeID>> minQs = stellar::LocalNode::findMinQuorum(localNodeID, herder.getCurrentlyTrackedQuorum());
        bool inclusionResult = stellar::LocalNode::isQuorumInclusion(minQs, newQ);
        auto peers = static_cast<OverlayManagerImpl&>(app.getOverlayManager()).getAuthenticatedPeers();
        //if(inclusionResult){
        //If the inclusion check pass, send ack message to requesting node
        if (mAdd.destination == localNodeID) {
            peers.begin()->second->sendInclusion(inclusionResult, mAdd.destination, mAdd.qSet.validators, localNodeID);
        } else {
            peers.find(mAdd.destination)->second->sendInclusion(inclusionResult, mAdd.destination, mAdd.qSet.validators, localNodeID);
        }    
        //} else {
        //If the inclusion check fails, send nack message to requesting node
            //peers.find(mAdd.destination)->second->sendInclusion(false, mAdd.destination, mAdd.qSet.validators, localNodeID);
        //}
    }
        
    innerResult().code(ADD_SUCCESS);
    return true;
}

bool
AddOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    // we do not need validity check at this time.
    //potentially, we can check whether qSet is the quorum system of destination
    //if (mAdd.destination == getSourceID())
    //{
        //innerResult().code(ADD_MALFORMED);
        //return false;
    //}

    return true;
}

void
AddOpFrame::insertLedgerKeysToPrefetch(
    UnorderedSet<LedgerKey>& keys) const
{
    //For add operation, we do not need to modify the database
    //keys.emplace(accountKey(mCreateAccount.destination));
}
}
