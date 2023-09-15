// Copyright 2020 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ledger/LedgerTxn.h"
#include "lib/catch.hpp"
#include "main/Application.h"
#include "main/Config.h"
#include "test/TestAccount.h"
#include "test/TestExceptions.h"
#include "test/TestMarket.h"
#include "test/TestUtils.h"
#include "test/TxTests.h"
#include "test/test.h"
#include "transactions/SignatureUtils.h"
#include "transactions/TransactionFrameBase.h"
#include "transactions/TransactionUtils.h"
#include "transactions/test/SponsorshipTestUtils.h"

using namespace stellar;
using namespace stellar::txtest;

static LeaveResultCode
getLeaveResultCode(TransactionFrameBasePtr& tx, size_t i)
{
    auto const& opRes = tx->getResult().result.results()[i];
    return opRes.tr().leaveResult().code();
}

TEST_CASE("leave", "[tx][leave]")
{
    VirtualClock clock;
    auto app = createTestApplication(clock, getTestConfig());

    // set up world
    auto root = TestAccount::createRoot(*app);

    //int64_t const txfee = app->getLedgerManager().getLastTxFee();
    //int64_t const minBalance2 = app->getLedgerManager().getLastMinBalance(2) + 10 * txfee;

    //SECTION("malformed with destination")
    //{
    //    auto tx =
    //        transactionFrameFromOps(app->getNetworkID(), root,
    //                                {root.op(leave(root, testQSet(0, 2)))}, {});
    //
    //        LedgerTxn ltx(app->getLedgerTxnRoot());
    //        REQUIRE(!tx->checkValid(*app, ltx, 0, 0, 0));
    //        REQUIRE(getLeaveResultCode(tx, 0) ==
    //                LEAVE_MALFORMED);
    //}

    SECTION("Success")
    {
        //root.leaveNetwork("B", app->getLedgerManager().getLastMinBalance(0));
        //root.leaveNetwork("B", testQSet(0, 2));
    };

}
