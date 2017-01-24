#include "girls_senriyama.h"
#include "table.h"
#include "ai.h"
#include "string_enum.h"
#include "util.h"

#include <algorithm>
#include <cassert>



namespace saki
{




bool Toki::checkInit(Who who, const Hand &init, const Princess &princess, int iter)
{
    (void) princess;

    return who != mSelf || iter > 10 || init.step4() <= 4;
}

void Toki::onDraw(const Table &table, Mount &mount, Who who, bool rinshan)
{
    if (who != mSelf || rinshan)
        return;

    accelerate(mount, table.getHand(mSelf), table.getRiver(mSelf), 25);
}

void Toki::onActivate(const Table &table, TicketFolder &tickets)
{
    (void) table;

    if (mToRiichi)
        return;

    if (tickets.can(ActCode::DICE) || tickets.spinOnly())
        return; // not a branch point

    if (mCd > 0) {
        mCd--;
    } else {
        mCleanTickets = tickets;
        tickets.enable(ActCode::IRS_CLICK);
        mCrazyTickets = tickets;
        mCrazyTickets.setForwarding(true);
    }
}

void Toki::onInbox(Who who, const Action &action)
{
    // range of care, excluding RIICHI, DICE, and IRS
    static const std::vector<ActCode> CHECK {
        ActCode::PASS, ActCode::SWAP_OUT, ActCode::SPIN_OUT,
        ActCode::CHII_AS_LEFT, ActCode::CHII_AS_MIDDLE, ActCode::CHII_AS_RIGHT,
        ActCode::PON, ActCode::DAIMINKAN, ActCode::ANKAN, ActCode::KAKAN,
        ActCode::TSUMO, ActCode::RON, ActCode::RYUUKYOKU
    };

    if (who != mSelf || !mCheckNextAction)
        return;

    if (action.act() == ActCode::RIICHI) {
        mToRiichi = true;
    } else if (util::has(CHECK, action.act())) {
        mCheckNextAction = false;

        std::pair<ActCode, int> move(action.act(), action.encodeArg());
        if (mToRiichi) {
            mToRiichi = false;
            move.first = ActCode::RIICHI; // special mark
        }

        auto it = std::find(mRecords.begin(), mRecords.end(), move);
        mCd = 2 * (it - mRecords.begin());
        mRecords.clear();
    }
}

TicketFolder Toki::forwardAction(const Table &table, Mount &mount, const Action &action)
{
    if (action.act() == ActCode::IRS_CLICK) {
        popUpBy(table, PopUpMode::GREEN);
        if (mInFuture) { // to exit future
            mInFuture = false;
            mCheckNextAction = true;
            return mCleanTickets;
        } else { // to enter future
            mInFuture = true;
            return mCrazyTickets;
        }
    } else if (action.act() == ActCode::RIICHI) {
        TicketFolder tickets;
        tickets.setForwarding(true);

        bool spinnable;
        std::vector<T37> swappables;
        table.getHand(mSelf).declareRiichi(swappables, spinnable);

        if (!swappables.empty())
            tickets.enableSwapOut(swappables);
        if (spinnable)
            tickets.enable(ActCode::SPIN_OUT);

        mToRiichi = true;
        return tickets;
    }

    popUpBy(table, PopUpMode::OO);

    // prepare operators
    std::array<Girl::Id, 4> ids;
    for (int w = 0; w < 4; w++)
        ids[w] = table.getGirl(Who(w)).getId();
    std::array<std::unique_ptr<TableOperator>, 4> ais = TokiAutoOp::create(ids, action);
    std::array<TableOperator*, 4> operators;
    std::transform(ais.begin(), ais.end(), operators.begin(),
                   [](std::unique_ptr<TableOperator> &up) { return up.get(); });

    // prepare observer
    TokiMountTracker mountTracker(mount, mSelf);
    std::vector<TableObserver*> observers { &mountTracker };

    Table future(table, operators, observers, mSelf, mCleanTickets);
    std::pair<ActCode, int> record(action.act(), action.encodeArg());

    if (mToRiichi) {
        mToRiichi = false;
        record.first = ActCode::RIICHI; // special mark
        future.action(mSelf, Action(ActCode::RIICHI));
    } else {
        future.start();
    }

    // push iff not found
    if (!util::has(mRecords, record))
        mRecords.push_back(record);

    mEvents = mountTracker.getEvents();
    popUpBy(table, PopUpMode::FV);
    return mCrazyTickets;
}

std::string Toki::popUpStr() const
{
    switch (mPopUpMode) {
    case PopUpMode::GREEN:
        return std::string("GREEN");
    case PopUpMode::OO:
        return std::string("----  O.O  ----");
    case PopUpMode::FV:
        return mEvents.str();
    default:
        unreached("Toki::popUpStr");
    }
}

void Toki::popUpBy(const Table &table, Toki::PopUpMode mode)
{
    mPopUpMode = mode;
    table.popUp(mSelf);
}



} // namespace saki


