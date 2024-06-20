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
#include "transactions/LeaveOpFrame.h"
#include "herder/HerderImpl.h"
#include "herder/QuorumTracker.h"
#include "scp/SCP.h"
#include "xdr/Stellar-ledger.h"

#include "scp/QuorumSetUtils.h"
#include "scp/LocalNode.h"
#include "util/XDROperators.h"
#include "xdr/Stellar-SCP.h"
#include "xdr/Stellar-types.h"
#include "herder/Herder.h"
#include <set>
#include "util/Logging.h"

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
    //VirtualClock clock;
    //auto app = createTestApplication(clock, getTestConfig(0, Config::TESTDB_ON_DISK_SQLITE));
    Config cfg(getTestConfig(0, Config::TESTDB_ON_DISK_SQLITE));
    cfg.MANUAL_CLOSE = false;

    std::vector<SecretKey> otherKeys;
    int const kKeysCount = 4;
    for (int i = 0; i < kKeysCount; i++)
    {
        otherKeys.emplace_back(SecretKey::pseudoRandomForTesting());
    }

    auto buildQSet = [&](int i) {
        SCPQuorumSet q;
        q.threshold = 2;
        q.validators.emplace_back(otherKeys[i].getPublicKey());
        q.validators.emplace_back(otherKeys[i + 1].getPublicKey());
        return q;
    };

    //for the current node {2, 3} 
    cfg.QUORUM_SET = buildQSet(2);
    auto qSet2 = buildQSet(2);
    //node 0: {0, 1}
    auto qSet0 = buildQSet(0);
    //node 1: {1, 2}
    auto qSet1 = buildQSet(1);
    //node 3: {3, 0}
    SCPQuorumSet qSet3;
    qSet3.threshold = 2;
    qSet3.validators.emplace_back(otherKeys[3].getPublicKey());
    qSet3.validators.emplace_back(otherKeys[0].getPublicKey());


    auto clock = std::make_shared<VirtualClock>();
    Application::pointer app = createTestApplication(*clock, cfg);

    auto* herder = static_cast<HerderImpl*>(&app->getHerder());
    auto* penEnvs = &herder->getPendingEnvelopes();

    // allow SCP messages from other slots to be processed
    herder->lostSync();

    auto valSigner = SecretKey::pseudoRandomForTesting();

    struct ValuesTxSet
    {
        Value mSignedV;
        TxSetFrameConstPtr mTxSet;
    };

    auto recvEnvelope = [&](SCPEnvelope envelope, uint64 slotID,
                            SecretKey const& k, SCPQuorumSet const& qSet,
                            std::vector<ValuesTxSet> const& pp) {
        // herder must want the TxSet before receiving it, so we are sending it
        // fake envelope
        envelope.statement.slotIndex = slotID;
        auto qSetH = sha256(xdr::xdr_to_opaque(qSet));
        envelope.statement.nodeID = k.getPublicKey();
        envelope.signature = k.sign(xdr::xdr_to_opaque(
            app->getNetworkID(), ENVELOPE_TYPE_SCP, envelope.statement));
        herder->recvSCPEnvelope(envelope);
        herder->recvSCPQuorumSet(qSetH, qSet);
        for (auto& p : pp)
        {
            herder->recvTxSet(p.mTxSet->getContentsHash(), p.mTxSet);
        }
    };
    auto recvNom = [&](uint64 slotID, SecretKey const& k,
                       SCPQuorumSet const& qSet,
                       std::vector<ValuesTxSet> const& pp) {
        SCPEnvelope envelope;
        envelope.statement.pledges.type(SCP_ST_NOMINATE);
        auto& nom = envelope.statement.pledges.nominate();

        std::set<Value> values;
        for (auto& p : pp)
        {
            values.insert(p.mSignedV);
        }
        nom.votes.insert(nom.votes.begin(), values.begin(), values.end());
        auto qSetH = sha256(xdr::xdr_to_opaque(qSet));
        nom.quorumSetHash = qSetH;
        recvEnvelope(envelope, slotID, k, qSet, pp);
    };
    auto makeValue = [&](int i) {
        auto const& lcl = app->getLedgerManager().getLastClosedLedgerHeader();
        auto txSet = TxSetFrame::makeEmpty(lcl);
        StellarValue sv = herder->makeStellarValue(
            txSet->getContentsHash(), lcl.header.scpValue.closeTime + i,
            emptyUpgradeSteps, valSigner);
        auto v = xdr::xdr_to_opaque(sv);
        return ValuesTxSet{v, txSet};
    };

    auto vv = makeValue(1);

    auto checkInQuorum = [&](std::set<int> ids) {
        REQUIRE(
            penEnvs->isNodeDefinitelyInQuorum(cfg.NODE_SEED.getPublicKey()));
        for (int j = 0; j < kKeysCount; j++)
        {
            bool inQuorum = (ids.find(j) != ids.end());
            REQUIRE(penEnvs->isNodeDefinitelyInQuorum(
                        otherKeys[j].getPublicKey()) == inQuorum);
        }
    };

    // set up world
    auto root = TestAccount::createRoot(*app);

    //int64_t const txfee = app->getLedgerManager().getLastTxFee();
    //int64_t const minBalance2 = app->getLedgerManager().getLastMinBalance(2) + 10 * txfee;


    SECTION("Leave request fail: no intersection")
    {
    //    auto tx =
    //        transactionFrameFromOps(app->getNetworkID(), root,
    //                                {root.op(leave(root, testQSet(0, 2)))}, {});
    //
    //        LedgerTxn ltx(app->getLedgerTxnRoot());
    //        REQUIRE(!tx->checkValid(*app, ltx, 0, 0, 0));
    //        REQUIRE(getLeaveResultCode(tx, 0) ==
    //                LEAVE_MALFORMED);

        //0: {0, 1}
        SCPQuorumSet qSet01;
        qSet01.threshold = 2;
        qSet01.validators.emplace_back(otherKeys[0].getPublicKey());
        qSet01.validators.emplace_back(otherKeys[1].getPublicKey());
        //1: {1, 2}
        SCPQuorumSet qSet12;
        qSet12.threshold = 2;
        qSet12.validators.emplace_back(otherKeys[1].getPublicKey());
        qSet12.validators.emplace_back(otherKeys[2].getPublicKey());
        //3: {2, 3}
        SCPQuorumSet qSet23;
        qSet23.threshold = 2;
        qSet23.validators.emplace_back(otherKeys[2].getPublicKey());
        qSet23.validators.emplace_back(otherKeys[3].getPublicKey());
        //2: {2, 3}, {2, 0}
        SCPQuorumSet qSet20;
        qSet20.threshold = 2;
        qSet20.validators.emplace_back(otherKeys[2].getPublicKey());
        qSet20.validators.emplace_back(otherKeys[0].getPublicKey());
        SCPQuorumSet qSet2Out;
        qSet2Out.threshold = 2;
        qSet2Out.validators.emplace_back(otherKeys[0].getPublicKey());
        qSet2Out.validators.emplace_back(otherKeys[2].getPublicKey());
        qSet2Out.validators.emplace_back(otherKeys[3].getPublicKey());
        //qSet2Out.innerSets.emplace_back(qSet20);
        //qSet2Out.innerSets.emplace_back(qSet23);
        //self: {2}
        SCPQuorumSet qSetSelf;
        qSetSelf.threshold = 1;
        qSetSelf.validators.emplace_back(otherKeys[2].getPublicKey());

        cfg.QUORUM_SET = qSetSelf;
        app.reset();
        clock.reset();

        clock = std::make_shared<VirtualClock>();
        app = Application::create(*clock, cfg, false);
        app->start();
        herder = static_cast<HerderImpl*>(&app->getHerder());
        penEnvs = &herder->getPendingEnvelopes();
        herder->lostSync();

        //receive self
        auto vv2 = makeValue(2);
        recvNom(3, cfg.NODE_SEED, qSetSelf, {vv, vv2});
        recvNom(3, otherKeys[2], qSet2Out, {vv, vv2});
        recvNom(3, otherKeys[3], qSet23, {vv, vv2});
        recvNom(3, otherKeys[0], qSet01, {vv, vv2});
        recvNom(3, otherKeys[1], qSet12, {vv, vv2});

        std::vector<std::vector<NodeID>> minQTest = stellar::LocalNode::findMinQuorum(cfg.NODE_SEED.getPublicKey(), herder->getCurrentlyTrackedQuorum());
        std::set<int> ids = {2, 3};
        for (int j = 0; j < kKeysCount; j++)
        {
            bool inQuorum = (ids.find(j) != ids.end());
            bool inMinQ = (std::find(minQTest[0].begin(), minQTest[0].end(), otherKeys[j].getPublicKey()) != minQTest[0].end());
            REQUIRE(inMinQ == inQuorum);
        }
    
        //create a quorum set to store calculated minimal quorums based on the current process
        SCPQuorumSet qSetMinQ;
        qSetMinQ.threshold = 2;
        for(auto it : minQTest){
            SCPQuorumSet defaultQ;
            defaultQ.threshold = 2;
            for(auto id: it){
                defaultQ.validators.emplace_back(id);
            }
            qSetMinQ.innerSets.emplace_back(defaultQ);
        }
        auto root2 = TestAccount::createRoot(*app);
        root2.leaveNetwork(otherKeys[2], qSetMinQ);
        std::set<NodeID> currentTomb = herder->getSCP().getLocalNode()->getTombSet();
        REQUIRE(currentTomb.find(otherKeys[2].getPublicKey()) == currentTomb.end());
        std::vector<std::vector<NodeID>> updatedMinQs = stellar::LocalNode::findMinQuorum(cfg.NODE_SEED.getPublicKey(), herder->getCurrentlyTrackedQuorum());
        REQUIRE(std::find(updatedMinQs[0].begin(), updatedMinQs[0].end(), otherKeys[2].getPublicKey()) != updatedMinQs[0].end());
        //auto tx = transactionFrameFromOps(app->getNetworkID(), root2,{root2.op(leave(otherKeys[2].getPublicKey(), qSetMinQ))}, {});
        //REQUIRE(getLeaveResultCode(tx, 0) == LEAVE_MALFORMED);

        // test for normal transactions after leave operations
        auto b1 = root2.create("B", app->getLedgerManager().getLastMinBalance(0));
        REQUIRE_THROWS_AS(root2.create("B", app->getLedgerManager().getLastMinBalance(0)), ex_CREATE_ACCOUNT_ALREADY_EXIST);
    }
    

    SECTION("Success")
    {
        SCPQuorumSet qSetSelf;
        qSetSelf.threshold = 1;
        qSetSelf.validators.emplace_back(otherKeys[2].getPublicKey());

        cfg.QUORUM_SET = qSetSelf;
        app.reset();
        clock.reset();

        clock = std::make_shared<VirtualClock>();
        app = Application::create(*clock, cfg, false);
        app->start();
        herder = static_cast<HerderImpl*>(&app->getHerder());
        penEnvs = &herder->getPendingEnvelopes();
        herder->lostSync();

        recvNom(3, cfg.NODE_SEED, cfg.QUORUM_SET, {vv});
        recvNom(3, otherKeys[2], qSet2, {vv});
        checkInQuorum({2, 3});
        //expand 0, 1, 3
        recvNom(3, otherKeys[3], qSet3, {vv});
        checkInQuorum({0, 2, 3});
        recvNom(3, otherKeys[0], qSet0, {vv});
        checkInQuorum({0, 1, 2, 3});
        recvNom(3, otherKeys[1], qSet1, {vv});
        checkInQuorum({0, 1, 2, 3});


        //root.leaveNetwork("B", testQSet(0, 2));

        //create a quorum set to store calculated minimal quorums based on the current process
        SCPQuorumSet qSetMinQ;
        qSetMinQ.threshold = 2;
        std::vector<std::vector<NodeID>> minQs = stellar::LocalNode::findMinQuorum(cfg.NODE_SEED.getPublicKey(), herder->getCurrentlyTrackedQuorum());
        for(auto it : minQs){
            SCPQuorumSet defaultQ;
            defaultQ.threshold = 2;
            for(auto id: it){
                defaultQ.validators.emplace_back(id);
            }
            qSetMinQ.innerSets.emplace_back(defaultQ);
        }
        auto root3 = TestAccount::createRoot(*app);
        root3.leaveNetwork(otherKeys[2], qSetMinQ);

        std::vector<std::vector<NodeID>> updatedMinQs = stellar::LocalNode::findMinQuorum(cfg.NODE_SEED.getPublicKey(), herder->getCurrentlyTrackedQuorum());
        std::set<int> ids = {0, 1, 3};
        for (int j = 0; j < kKeysCount; j++)
        {
            bool inQuorum = (ids.find(j) != ids.end());
            bool inMinQ = (std::find(updatedMinQs[0].begin(), updatedMinQs[0].end(), otherKeys[j].getPublicKey()) != updatedMinQs[0].end());
            REQUIRE(inMinQ == inQuorum);
        }
        std::set<NodeID> currentTomb = herder->getSCP().getLocalNode()->getTombSet();
        REQUIRE(currentTomb.find(otherKeys[2].getPublicKey()) != currentTomb.end());
        REQUIRE(std::find(updatedMinQs[0].begin(), updatedMinQs[0].end(), otherKeys[2].getPublicKey()) == updatedMinQs[0].end());

        // test for normal transactions after leave operations
        auto b1 = root3.create("B", app->getLedgerManager().getLastMinBalance(0));
        REQUIRE_THROWS_AS(root3.create("B", app->getLedgerManager().getLastMinBalance(0)), ex_CREATE_ACCOUNT_ALREADY_EXIST);
    }
}
