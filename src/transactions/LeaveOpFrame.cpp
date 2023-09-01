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


bool
LeaveOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    ZoneNamedN(applyZone, "LeaveOp apply", true);
    //We do not modify the ledger for leave operation
    //We may need to modify quorums later
    innerResult().code(LEAVE_SUCCESS);

    return true;
}

bool
LeaveOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    //int64_t minStartingBalance =
        //protocolVersionStartsFrom(ledgerVersion, ProtocolVersion::V_14) ? 0 : 1;
    //if (mCreateAccount.startingBalance < minStartingBalance)
    //{
        //innerResult().code(CREATE_ACCOUNT_MALFORMED);
        //return false;
    //}

    if (mLeave.destination == getSourceID())
    {
        innerResult().code(LEAVE_MALFORMED);
        return false;
    }

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
