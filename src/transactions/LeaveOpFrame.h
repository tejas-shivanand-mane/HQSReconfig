#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/OperationFrame.h"

namespace stellar
{

class AbstractLedgerTxn;

class LeaveOpFrame : public OperationFrame
{
    LeaveResult&
    innerResult()
    {
        return mResult.tr().leaveResult();
    }
    LeaveOp const& mLeave;

    //bool doApplyBeforeV14(AbstractLedgerTxn& ltx);
    //bool doApplyFromV14(AbstractLedgerTxn& ltxOuter);

    //bool checkLowReserve(AbstractLedgerTxn& ltx);
    //bool deductStartingBalance(AbstractLedgerTxn& ltx);
    //void createAccount(AbstractLedgerTxn& ltx);

  public:
    LeaveOpFrame(Operation const& op, OperationResult& res,
                         TransactionFrame& parentTx);

    bool doApply(AbstractLedgerTxn& ltx) override;
    bool doCheckValid(uint32_t ledgerVersion) override;
    void
    insertLedgerKeysToPrefetch(UnorderedSet<LedgerKey>& keys) const override;

    static LeaveResultCode
    getInnerCode(OperationResult const& res)
    {
        return res.tr().leaveResult().code();
    }
};
}
