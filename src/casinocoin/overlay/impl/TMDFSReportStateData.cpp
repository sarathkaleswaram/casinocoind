//------------------------------------------------------------------------------
/*
    This file is part of casinocoind: https://github.com/casinocoin/casinocoind
    Copyright (c) 2018 Casinocoin Foundation

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

//==============================================================================
/*
    2018-06-04  jrojek        created
*/
//==============================================================================

#include "TMDFSReportStateData.h"
#include <casinocoin/overlay/impl/OverlayImpl.h>

namespace casinocoin {

TMDFSReportStateData::TMDFSReportStateData(OverlayImpl& overlay,
                                           beast::Journal journal)
    : overlay_(overlay)
    , journal_(journal)
{
}

void TMDFSReportStateData::restartTimer(std::string const& initiatorPubKey,
                                                    std::string const& currRecipient,
                                                    protocol::TMDFSReportState const& currPayload)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if (dfsTimers_.find(initiatorPubKey) == dfsTimers_.end())
        dfsTimers_[initiatorPubKey] = std::make_unique<DeadlineTimer>(this);

    dfsTimers_[initiatorPubKey]->setExpiration(1s);
    lastReqRecipient_[initiatorPubKey] = currRecipient;
    lastReq_[initiatorPubKey] = currPayload;
}

void TMDFSReportStateData::cancelTimer(std::string const& initiatorPubKey)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if (dfsTimers_.find(initiatorPubKey) != dfsTimers_.end())
        dfsTimers_[initiatorPubKey]->cancel();
    else
        JLOG(journal_.warn()) << "TMDFSReportStateData::cancelTimer couldn't find timer for root node: "
                              << initiatorPubKey;
}

protocol::TMDFSReportState& TMDFSReportStateData::getLastRequest(std::string const& initiatorPubKey)
{
    return lastReq_[initiatorPubKey];
}

std::string& TMDFSReportStateData::getLastRecipient(std::string const& initiatorPubKey)
{
    return lastReqRecipient_[initiatorPubKey];
}

void TMDFSReportStateData::onDeadlineTimer(DeadlineTimer &timer)
{
    // jrojek this might be because node just recently gone offline
    // or because node does not support CRN feature. Either way, we decide that this node is already
    // visited and do not account its state
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    timer.cancel();

    std::string initiator;
    for (auto iter = dfsTimers_.begin(); iter != dfsTimers_.end(); ++iter)
    {
        if (*(iter->second) == timer)
        {
            initiator = iter->first;
            break;
        }
    }
    lastReq_[initiator].add_visited(lastReqRecipient_[initiator]);
    lastReq_[initiator].set_type(protocol::TMDFSReportState::rtRESP);

    Overlay::PeerSequence knownPeers = overlay_.getActivePeers();
    if (knownPeers.size() > 0)
    // jrojek need to call that on any instance of TMDFSReportState as this is basically callback to 'me'
    {
        knownPeers[0]->dfsReportState().evaluateResponse(std::make_shared<protocol::TMDFSReportState>(lastReq_[initiator]));
    }
    lastReq_[initiator].set_type(protocol::TMDFSReportState::rtREQ);
}

}