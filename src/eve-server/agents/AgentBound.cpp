
 /**
  * @name AgentBound.cpp
  *   agent specific code
  *    removed from AgentMgrService.cpp
  *
  * @Author:        Allan
  * @date:      26 June 2018
  *
  */


/*
 * # Agent Logging:
 * AGENT__ERROR
 * AGENT__WARNING
 * AGENT__MESSAGE
 * AGENT__DEBUG
 * AGENT__INFO
 * AGENT__TRACE
 * AGENT__DUMP
 * AGENT__RSP_DUMP
 */

#include "eve-server.h"
#include "../../eve-common/EVE_Missions.h"
//#include "../../eve-common/EVE_Skills.h"
#include "../../eve-common/EVE_Standings.h"

#include "StaticDataMgr.h"
#include "account/AccountService.h"
#include "corporation/LPService.h"
#include "agents/AgentBound.h"
#include "agents/AgentMgrService.h"
#include "station/Station.h"
#include "services/ServiceManager.h"

AgentBound::AgentBound(EVEServiceManager& mgr, AgentMgrService& parent, Agent *agt) :
    EVEBoundObject(mgr, parent),
    m_agent(agt)
{
    this->Add("DoAction", &AgentBound::DoAction);
    this->Add("GetAgentLocationWrap", &AgentBound::GetAgentLocationWrap);
    this->Add("GetInfoServiceDetails", &AgentBound::GetInfoServiceDetails);
    this->Add("GetMissionBriefingInfo", &AgentBound::GetMissionBriefingInfo);
    this->Add("GetMissionObjectiveInfo", &AgentBound::GetMissionObjectiveInfo);
    this->Add("GetMissionKeywords", &AgentBound::GetMissionKeywords);
    this->Add("GetMissionJournalInfo", &AgentBound::GetMissionJournalInfo);
    this->Add("GetDungeonShipRestrictions", &AgentBound::GetDungeonShipRestrictions);
    this->Add("RemoveOfferFromJournal", &AgentBound::RemoveOfferFromJournal);
    this->Add("GetOfferJournalInfo", &AgentBound::GetOfferJournalInfo);
    this->Add("GetEntryPoint", &AgentBound::GetEntryPoint);
    this->Add("GotoLocation", &AgentBound::GotoLocation);
    this->Add("WarpToLocation", &AgentBound::WarpToLocation);
    this->Add("GetMyJournalDetails", &AgentBound::GetMyJournalDetails);
}
PyResult AgentBound::GetAgentLocationWrap(PyCallArgs &call) {
    // this is detailed info on agent's location
    return m_agent->GetLocationWrap();
}

PyResult AgentBound::GetInfoServiceDetails(PyCallArgs& call) {
    // this is agents personal info... level, division, station, etc.
    return m_agent->GetInfoServiceDetails();
}


/**     ***********************************************************************
 * @note   these below are partially coded
 */

PyResult AgentBound::DoAction(PyCallArgs &call, std::optional <PyInt*> actionID) {
    // this is first call when initiating agent convo
    _log(AGENT__DUMP,  "AgentBound::Handle_DoAction() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    // this actually returns a complicated tuple depending on other variables involving this agent and char.
    /*
        agentSays, dialogue, extraInfo = self.__GetConversation(agentDialogueWindow, actionID)

    def __GetConversation(self, wnd, actionID):
       tmp = wnd.sr.agentMoniker.DoAction(actionID)
        ret, wnd.sr.oob = tmp
        agentSays, wnd.sr.dialogue = ret
        firstActionID = wnd.sr.dialogue[0][0]
        firstActionDialogue = wnd.sr.dialogue[0][1]
        wnd.sr.agentSays = self.ProcessMessage(agentSays, wnd.sr.agentID)
        return (wnd.sr.agentSays, wnd.sr.dialogue, wnd.sr.oob)
     *
     */

    Character* pchar = call.client->GetChar().get();
    float charStanding = StandingDB::GetStanding(m_agent->GetID(), pchar->itemID());
    float quality = EvEMath::Agent::EffectiveQuality(m_agent->GetQuality(), pchar->GetSkillLevel(EvESkill::Connections), charStanding);
    float bonus = EvEMath::Agent::GetStandingBonus(charStanding, m_agent->GetFactionID(), pchar->GetSkillLevel(EvESkill::Connections), pchar->GetSkillLevel(EvESkill::Diplomacy), pchar->GetSkillLevel(EvESkill::CriminalConnections));
    float standing = EvEMath::Agent::EffectiveStanding(charStanding, bonus);

    std::string response = "";
    bool missionQuit = false, missionCompleted = false, missionDeclined = false;

    PyTuple* agentSays = new PyTuple(2);
    // dialog is button info
    PyList* dialog = new PyList();

    using namespace Dialog::Button;

    // to set 'admin dialog options' (which i dont know wtf they are yet), i *think* you add a tuple of *something* that is NOT dict or int.
    if (false /*admin options*/) {
        PyTuple* adminButton = new PyTuple(2);
        adminButton->SetItem(0, new PyInt(Admin));
        adminButton->SetItem(1, new PyString("Admin Options"));
        dialog->AddItem(adminButton);
    }

    if (m_agent->CanUseAgent(call.client)) {
        switch (actionID.has_value() ? actionID.value()->value() : 0) {
            case 0: {
                //  if char has current mission with this agent, add this one.
                MissionOffer offer = MissionOffer();
                if (m_agent->HasMission(pchar->itemID(), offer)) {
                    PyTuple* button1 = new PyTuple(2);
                        button1->SetItem(0, new PyInt(ViewMission)); // this are buttonIDs which are unique and sequential to each agent, regardless of chars
                        button1->SetItem(1, new PyInt(ViewMission));
                    dialog->AddItem(button1);
                    if (call.client->IsMissionComplete(offer))  {
                        PyTuple* button2 = new PyTuple(2);
                            button2->SetItem(0, new PyInt(Complete));
                            button2->SetItem(1, new PyInt(Complete));
                        dialog->AddItem(button2);
                    }
                    agentSays->SetItem(0, new PyInt(offer.briefingID));
                    agentSays->SetItem(1, new PyInt(offer.offerID));
                } else {
                    // dialogue data.  if RequestMission is only option, client auto-responds with DoAction(RequestMission optionID)
                    PyTuple* button2 = new PyTuple(2);
                        button2->SetItem(0, new PyInt(RequestMission));
                        button2->SetItem(1, new PyInt(RequestMission));
                    dialog->AddItem(button2);
                // response as string for custom data.  response as pyint to use client data (using getlocale shit)
                    // default initial agent response based on agent location, level, bloodline, quality, and char/agent standings
                    //  this will be modeled after UO speech data, in tiers and levels.
                    // if RequestMission is only option, this is ignored.  see note under 'dialog data'
                    response = "Why the fuck am I looking at you again, ";
                    response += call.client->GetName();
                    response += "?";
                    agentSays->SetItem(0, new PyString(response));  //msgInfo  -- if tuple[0].string then return msgInfo
                    agentSays->SetItem(1, PyStatic.NewNone());      // ContentID  -- PyNone used when msgInfo is string (mostly for initial greetings)
                }

                // if agent does location, add this one...
                if (m_agent->IsLocator()) {
                    PyTuple* button3 = new PyTuple(2);
                        button3->SetItem(0, new PyInt(LocateCharacter));
                        button3->SetItem(1, new PyInt(LocateCharacter));
                    dialog->AddItem(button3);
                }

                // if agent does research, add this one...
                if (m_agent->IsResearch()) {
                    PyTuple* button4 = new PyTuple(2);
                        button4->SetItem(0, new PyInt(StartResearch));
                        button4->SetItem(1, new PyInt(StartResearch));
                    dialog->AddItem(button4);
                }
            } break;
            case RequestMission: {  //2
                MissionOffer offer = MissionOffer();
                if (!m_agent->MakeOffer(pchar->itemID(), offer)) {
                    //  no mission data for this agent's level, or nowhere to send the player.
                    agentSays->SetItem(0, new PyString("I have nothing for you right now.  Check back with me later."));
                    agentSays->SetItem(1, PyStatic.NewNone());
                    break;
                }
                m_agent->SendMissionUpdate(call.client, "offered");

                //  this one will get complicated and is based on agent/char interaction
                //   detail in /eve/client/script/ui/station/agents/agents.py

                /*  agentSays is a tuple of msgData and contentID
                *      msgData can be single integer of briefingID, a string literal, or a tuple as defined above.
                *   contentID is used for specific char's mission keywords.  we're not using it like there here....
                */

                agentSays->SetItem(0, new PyInt(offer.briefingID));
                agentSays->SetItem(1, new PyInt(offer.offerID));

                // dialog can also contain mission data.
                //   set a dialog tuple[1] to dict and fill with MissionBriefingInfo
                PyTuple* button1 = new PyTuple(2);
                    button1->SetItem(0, new PyInt(Accept));
                    button1->SetItem(1, new PyInt(Accept));
                dialog->AddItem(button1);
                PyTuple* button2 = new PyTuple(2);
                    button2->SetItem(0, new PyInt(Decline));
                    button2->SetItem(1, new PyInt(Decline));
                dialog->AddItem(button2);
                PyTuple* button3 = new PyTuple(2);
                    button3->SetItem(0, new PyInt(Defer));
                    button3->SetItem(1, new PyInt(Defer));
                dialog->AddItem(button3);
            } break;
            case ViewMission: { //1
                MissionOffer offer = MissionOffer();
                /*  GetOffer() only warns on a miss and leaves the struct zeroed, which would send
                 *  briefingID 0 with contentID 0 - and a non-null contentID makes the client call
                 *  GetMissionKeywords(), which answers PyNone with no mission and blows up in
                 *  agents.py ProcessMessage().
                 */
                if (!m_agent->HasMission(pchar->itemID(), offer))
                    break;
                agentSays->SetItem(0, new PyInt(offer.briefingID));
                agentSays->SetItem(1, new PyInt(offer.offerID));
                if (offer.stateID < Mission::State::Accepted) {
                    PyTuple* button1 = new PyTuple(2);
                        button1->SetItem(0, new PyInt(Accept));
                        button1->SetItem(1, new PyInt(Accept));
                    dialog->AddItem(button1);
                    PyTuple* button2 = new PyTuple(2);
                        button2->SetItem(0, new PyInt(Decline));
                        button2->SetItem(1, new PyInt(Decline));
                    dialog->AddItem(button2);
                    PyTuple* button3 = new PyTuple(2);
                        button3->SetItem(0, new PyInt(Defer));
                        button3->SetItem(1, new PyInt(Defer));
                    dialog->AddItem(button3);
                } else if (offer.stateID == Mission::State::Accepted) {
                    PyTuple* button1 = new PyTuple(2);
                        button1->SetItem(0, new PyInt(Quit));
                        button1->SetItem(1, new PyInt(Quit));
                    dialog->AddItem(button1);
                    if (call.client->IsMissionComplete(offer))  {
                        PyTuple* button2 = new PyTuple(2);
                            button2->SetItem(0, new PyInt(Complete));
                            button2->SetItem(1, new PyInt(Complete));
                        dialog->AddItem(button2);
                    }
                }
            } break;
            case Accept:            //3
            case AcceptRemotely: {  //5
                MissionOffer offer = MissionOffer();
                /*  Agent::GetOffer() only logs when there is no offer, leaving a zeroed struct
                 *  behind, and re-accepting an already-accepted mission used to hand out a second
                 *  stack of courier goods and restart the bonus timer.  Both are reachable by
                 *  replaying the action, so check the state before touching anything.
                 */
                if (!m_agent->HasMission(pchar->itemID(), offer))
                    break;
                if (offer.stateID >= Mission::State::Accepted)
                    break;
                offer.stateID = Mission::State::Accepted;
                offer.dateAccepted = GetFileTimeNow();
                //  live gives a week to run a standard mission, regardless of agent level.
                offer.expiryTime = GetFileTimeNow() + EvE::Time::Week;
                if (offer.courierTypeID and offer.courierAmount) {
                    // add item to players hangar
                    sItemFactory.SetUsingClient(call.client);
                    ItemData data(offer.courierTypeID, pchar->itemID(), locTemp, flagNone, offer.courierAmount);
                    InventoryItemRef iRef = sItemFactory.SpawnItem(data);
                    if (iRef.get() != nullptr) {
                        iRef->Move(offer.originID, flagHangar, true);
                    } else {
                        _log(AGENT__ERROR, "AgentBound::DoAction() - failed to spawn courier item %u x%u for char %u",
                             offer.courierTypeID, offer.courierAmount, pchar->itemID());
                    }
                    sItemFactory.UnsetUsingClient();
                }
                m_agent->UpdateOffer(pchar->itemID(), offer);
                m_agent->SendMissionUpdate(call.client, "offer_accepted");
                //  canned agent line - carries no mission keywords, so send a null contentID.
                //  a non-null contentID makes the client call GetMissionKeywords() for it.
                agentSays->SetItem(0, new PyInt(m_agent->GetAcceptRsp(pchar->itemID())));
                agentSays->SetItem(1, PyStatic.NewNone());
            } break;
            case Complete:              //6
            case CompleteRemotely: {    //7
                MissionOffer offer = MissionOffer();
                if (!m_agent->HasMission(pchar->itemID(), offer))
                    break;
                /*  IsMissionComplete() decided whether to *draw* the Complete button, but nothing
                 *  checked it when the action came back, so replaying the call paid out the full
                 *  reward without ever picking the cargo up.
                 */
                if ((offer.stateID != Mission::State::Accepted) or (!call.client->IsMissionComplete(offer))) {
                    call.client->SendNotifyMsg("One or more mission objectives have not been completed.  Check your journal for details.");
                    agentSays->SetItem(0, new PyInt(offer.briefingID));
                    agentSays->SetItem(1, new PyInt(offer.offerID));
                    break;
                }
                missionCompleted = true;
                offer.stateID = Mission::State::Completed;
                offer.dateCompleted = GetFileTimeNow();
                m_agent->UpdateOffer(pchar->itemID(), offer);
                m_agent->SendMissionUpdate(call.client, "completed");
                agentSays->SetItem(0, new PyInt(m_agent->GetCompleteRsp(pchar->itemID())));
                agentSays->SetItem(1, PyStatic.NewNone());
                if (offer.courierTypeID) {
                    // remove item from player possession
                    call.client->RemoveMissionItem(offer.courierTypeID, offer.courierAmount);
                }
                if (offer.rewardItemID and offer.rewardItemQty) {
                    // add reward item to players hangar
                    sItemFactory.SetUsingClient(call.client);
                    ItemData data(offer.rewardItemID, pchar->itemID(), locTemp, flagNone, offer.rewardItemQty);
                    InventoryItemRef iRef = sItemFactory.SpawnItem(data);
                    if (iRef.get() != nullptr) {
                        //  an agent sitting in space has no stationID; dropping the reward at
                        //  locationID 0 would orphan it, so fall back to where the player is.
                        uint32 rewardLocation = (m_agent->GetStationID() ? m_agent->GetStationID() : call.client->GetLocationID());
                        iRef->Move(rewardLocation, flagHangar, true);
                    } else {
                        _log(AGENT__ERROR, "AgentBound::DoAction() - failed to spawn reward item %u x%u for char %u",
                             offer.rewardItemID, offer.rewardItemQty, pchar->itemID());
                    }
                    sItemFactory.UnsetUsingClient();
                }
                /** @todo  add fleet sharing  */
                if (offer.rewardISK)
                    AccountService::TransferFunds(m_agent->GetID(), pchar->itemID(), offer.rewardISK, "Mission Reward", Journal::EntryType::AgentMissionReward, m_agent->GetID());
                //  bonusTime is in minutes; compare it against how long the mission was actually held.
                if ((offer.bonusTime > 0) and (offer.bonusISK > 0)
                and ((offer.dateCompleted - offer.dateAccepted) < (offer.bonusTime * EvE::Time::Minute))) {
                    AccountService::TransferFunds(m_agent->GetID(), pchar->itemID(), offer.bonusISK, "Mission Bonus Reward", Journal::EntryType::AgentMissionTimeBonusReward, m_agent->GetID());
                    m_agent->UpdateStandings(call.client, Standings::MissionBonus, offer.important);
                }
                /** @todo  add lp, etc, etc  */
                if (offer.rewardLP)
                    LPService::AddLP(pchar->itemID(), m_agent->GetCorpID(), offer.rewardLP);
                m_agent->UpdateStandings(call.client, Standings::MissionCompleted, offer.important);
                //  UpdateOffer() above wrote the Completed state and completion date to agtOffers,
                //  which is all the history we keep.  Drop the offer from both live caches so it
                //  stops showing up in the journal and MissionDataMgr::Process() stops re-expiring
                //  it.  (Agent::RemoveOffer() only clears the agent's own copy.)
                sMissionDataMgr.RemoveMissionOffer(pchar->itemID(), offer);
                m_agent->RemoveOffer(pchar->itemID());
            } break;
            case Defer: {   //10
                // extend expiry time and close
                MissionOffer offer = MissionOffer();
                //  an accepted mission cannot be deferred - reverting it to Allocated would leave
                //  the courier goods in the player's hangar with the mission back on offer, and
                //  '+=' let each press stack another day on top.
                if (m_agent->HasMission(pchar->itemID(), offer) and (offer.stateID < Mission::State::Accepted)) {
                    offer.stateID = Mission::State::Allocated; //Defered
                    offer.expiryTime = GetFileTimeNow() + EvE::Time::Day;
                    m_agent->UpdateOffer(pchar->itemID(), offer);
                    m_agent->SendMissionUpdate(call.client, "prolong");
                    agentSays->SetItem(0, new PyString("I can give you 24 hours to think about it."));    //msgInfo  -- if tuple[0].string then return msgInfo
                    agentSays->SetItem(1, PyStatic.NewNone());    // ContentID  -- PyNone used when msgInfo is string to return without processing
                }
            } break;
            case Decline: { //9
                MissionOffer offer = MissionOffer();
                //  DeleteOffer() only warns when there is no offer, so a replayed decline used to
                //  keep applying the standings hit.
                if (!m_agent->HasMission(pchar->itemID(), offer))
                    break;
                missionDeclined = true;
                m_agent->DeleteOffer(pchar->itemID());
                m_agent->SendMissionUpdate(call.client, "offer_declined");
                agentSays->SetItem(0, new PyInt(m_agent->GetDeclineRsp(pchar->itemID())));
                agentSays->SetItem(1, PyStatic.NewNone());
                /** @todo  add lp, etc, etc  */
                m_agent->UpdateStandings(call.client, Standings::MissionDeclined);
            } break;
            case Quit: {    //11
                MissionOffer offer = MissionOffer();
                if (!m_agent->HasMission(pchar->itemID(), offer))
                    break;
                missionQuit = true;
                if (offer.courierTypeID) {
                    // remove item from player possession
                    call.client->RemoveMissionItem(offer.courierTypeID, offer.courierAmount);
                }
                // remove mission offer and set standings accordingly
                m_agent->DeleteOffer(pchar->itemID());
                m_agent->SendMissionUpdate(call.client, "quit");
                agentSays->SetItem(0, new PyInt(m_agent->GetQuitRsp(pchar->itemID())));
                agentSays->SetItem(1, PyStatic.NewNone());
                /** @todo  add lp, etc, etc  */
                m_agent->UpdateStandings(call.client, Standings::MissionFailure, offer.important);
            } break;
            case Continue: {    //8
                // closes the conversation; nothing to say.
                agentSays->SetItem(0, PyStatic.NewNone());
                agentSays->SetItem(1, PyStatic.NewNone());
            } break;
            case StartResearch: {   //12
                // not sure what to do here.
            } break;
            case CancelResearch: {  //13
                // not sure what to do here.
            } break;
            case BuyDatacores: {    //14
                // not sure what to do here.
            } break;
            case LocateCharacter:   //15
            case LocateAccept: {    //16
                // not sure what to do here.
            } break;
            case LocateReject: {    //17
                // not sure what to do here.
            } break;
            case Yes: {             //18
                // not sure what to do here.
            } break;
            case No: {              //19
                // not sure what to do here.
            } break;
            case AcceptChoice:{     //4
                // i think this is for options
            } break;
            case Admin: {           //20
                // not sure what to do here.
            } break;
            default: {
                /*  Returning nullptr here handed the client PyNone (Callable.cpp substitutes it),
                 *  and agents.py __GetConversation() unpacks the reply with "ret, sr.oob = tmp"
                 *  straight away - so a bad buttonID turned into a client-side TypeError.  Fall
                 *  through with an empty conversation instead; the reply keeps its normal shape.
                 */
                _log(AGENT__ERROR, "AgentBound::Handle_DoAction() - unhandled buttonID %u", (actionID.has_value() ? actionID.value()->value() : 0));
                call.client->SendErrorMsg("Internal Server Error. Ref: ServerError xxxxx.");
            } break;
        }
    } else {
        agentSays->SetItem(0, new PyInt(m_agent->GetStandingsRsp(pchar->itemID())));
        agentSays->SetItem(1, PyStatic.NewNone() /*new PyInt(pchar->itemID())*/);
    }

    // extraInfo data....
    PyDict* xtraInfo = new PyDict();
        xtraInfo->SetItemString("loyaltyPoints",    new PyInt(LPService::GetLPBalanceForCorp(pchar->itemID(),m_agent->GetCorpID())));  // this is char current LP
        xtraInfo->SetItemString("missionCompleted", new PyBool(missionCompleted));
        xtraInfo->SetItemString("missionQuit",      new PyBool(missionQuit));
        xtraInfo->SetItemString("missionDeclined",  new PyBool(missionDeclined));

    /*  PyTuple slots are born null and marshalling a null slot faults the server, so make sure both
     *  are populated before we build the reply.  PyTuple::empty() tests the slot *count* - which is
     *  always 2 here - so it never caught the cases below (Defer with no mission, StartResearch,
     *  CancelResearch, BuyDatacores, the Locate* buttons, Yes/No, AcceptChoice and Admin all fall
     *  through without touching agentSays).
     */
    if (agentSays->GetItem(0) == nullptr)
        agentSays->SetItem(0, PyStatic.NewNone());  // msgData:  briefingID, string literal or tuple
    if (agentSays->GetItem(1) == nullptr)
        agentSays->SetItem(1, PyStatic.NewNone());  // contentID
    PyTuple* inner = new PyTuple(2);
        inner->SetItem(0, agentSays);
        inner->SetItem(1, dialog);
    PyTuple* outer = new PyTuple(2);
        outer->SetItem(0, inner);
        outer->SetItem(1, xtraInfo);

    if (is_log_enabled(AGENT__RSP_DUMP)) {
        _log(AGENT__RSP_DUMP, "AgentBound::Handle_DoAction RSP:" );
        outer->Dump(AGENT__RSP_DUMP, "    ");
    }

    PySafeDecRef(agentSays);

    return outer;
}

PyResult AgentBound::GetMissionBriefingInfo(PyCallArgs &call) {
    // called from iniate agent convo... should be populated when mission available
    // will return PyNone if no mission avalible
    _log(AGENT__MESSAGE,  "AgentBound::Handle_GetMissionBriefingInfo()");

    MissionOffer offer = MissionOffer();
    if (!m_agent->HasMission(call.client->GetCharacterID(), offer))
        return PyStatic.NewNone();

    //  only suppress the briefing once the mission is actually over - returning None for an
    //  accepted mission stripped the title, expiry line and image out of the agent conversation
    //  window for the rest of the mission.  same list as GetMissionObjectiveInfo().
    switch (offer.stateID) {
        case Mission::State::Completed:
        case Mission::State::Rejected:
        case Mission::State::Expired: {
            return PyStatic.NewNone();
        }
    }

    // these are found in the client data by MessageIDs ....  i.e.  {[location]objectiveDestinationID.name}
    // contentID is the key for the keywords data on live...not used here
    PyDict* keywords = new PyDict();
        keywords->SetItemString("objectiveLocationID", new PyInt(offer.originID));
        keywords->SetItemString("objectiveLocationSystemID", new PyInt(offer.originSystemID));
        keywords->SetItemString("objectiveTypeID", new PyInt(offer.courierTypeID));
        keywords->SetItemString("objectiveQuantity", new PyInt(offer.courierAmount));
        keywords->SetItemString("objectiveDestinationID", new PyInt(offer.destinationID));
        keywords->SetItemString("objectiveDestinationSystemID", new PyInt(offer.destinationSystemID));
        if (offer.rewardISK) {
            keywords->SetItemString("rewardTypeID", new PyInt(itemTypeCredits));
            keywords->SetItemString("rewardQuantity", new PyInt(offer.rewardISK));
        } else {
            // wouldnt these be in 'extra' or ?
            keywords->SetItemString("rewardTypeID", new PyInt(offer.rewardItemID));
            keywords->SetItemString("rewardQuantity", new PyInt(offer.rewardItemQty));
        }
        keywords->SetItemString("dungeonLocationID", new PyInt(offer.dungeonLocationID));
        keywords->SetItemString("dungeonSolarSystemID", new PyInt(offer.dungeonSolarSystemID));
    PyDict *briefingInfo = new PyDict();
        briefingInfo->SetItemString("ContentID", new PyInt(offer.offerID));
        briefingInfo->SetItemString("Mission Keywords", keywords);
        //  missionID is the qstCourier/qstMining row id, not a locale messageID - send the name.
        briefingInfo->SetItemString("Mission Title ID", new PyString(offer.name));
        briefingInfo->SetItemString("Mission Briefing ID", new PyInt(offer.briefingID));
        //  agents.py __Interact() does briefingInformation['Mission Image'] with no guard, so the
        //  key has to exist for every mission type.
        switch(offer.typeID) {
            case Mission::Type::Mining:
                briefingInfo->SetItemString("Mission Image", sMissionDataMgr.GetMiningRes()); break;
            case Mission::Type::Encounter:
                briefingInfo->SetItemString("Mission Image", sMissionDataMgr.GetKillRes()); break;
            case Mission::Type::Courier:
            default:
                briefingInfo->SetItemString("Mission Image", sMissionDataMgr.GetCourierRes()); break;
        }
        // decline time OR expiration time.  if not decline then expiration
        briefingInfo->SetItemString("Decline Time", PyStatic.NewNone());   // -1 is generic decline msg
        briefingInfo->SetItemString("Expiration Time", new PyLong(offer.expiryTime) );

    if (is_log_enabled(AGENT__RSP_DUMP)) {
        _log(AGENT__RSP_DUMP, "AgentBound::Handle_GetMissionBriefingInfo() RSP:" );
        briefingInfo->Dump(AGENT__RSP_DUMP, "    ");
    }

    return briefingInfo;
}

PyResult AgentBound::GetMissionKeywords(PyCallArgs &call, PyInt* contentID) {
    // thse are the variables embedded in the messageIDs
    //self.missionArgs[contentID] = self.GetAgentMoniker(agentID).GetMissionKeywords(contentID)
    _log(AGENT__DUMP,  "AgentBound::Handle_GetMissionKeywords() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    /*   none of this really matters as we're not using 'contentID' like live does
    Call_SingleArg args;
    if (!args.Decode(&call.tuple)) {
        _log(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    uint32 contentID = PyRep::IntegerValueU32(args.arg);
    if (contentID == 0)
        return PyStatic.NewNone();
    */
    /*  agents.py ProcessMessage() stores this under self.missionArgs[contentID] and immediately
     *  calls .update() on it, so PyNone here is an AttributeError client side.  Hand back an empty
     *  dict when there is nothing to describe.
     */
    MissionOffer offer = MissionOffer();
    if (!m_agent->HasMission(call.client->GetCharacterID(), offer))
        return new PyDict();

    PyDict* keywords = new PyDict();
    keywords->SetItemString("objectiveLocationID", new PyInt(offer.originID));
    keywords->SetItemString("objectiveLocationSystemID", new PyInt(offer.originSystemID));
    keywords->SetItemString("objectiveTypeID", new PyInt(offer.courierTypeID));
    keywords->SetItemString("objectiveQuantity", new PyInt(offer.courierAmount));
    keywords->SetItemString("objectiveDestinationID", new PyInt(offer.destinationID));
    keywords->SetItemString("objectiveDestinationSystemID", new PyInt(offer.destinationSystemID));
    if (offer.rewardISK) {
        keywords->SetItemString("rewardTypeID", new PyInt(itemTypeCredits));
        keywords->SetItemString("rewardQuantity", new PyInt(offer.rewardISK));
    } else {
        // wouldnt these be in 'extra' or ?
        keywords->SetItemString("rewardTypeID", new PyInt(offer.rewardItemID));
        keywords->SetItemString("rewardQuantity", new PyInt(offer.rewardItemQty));
    }
    keywords->SetItemString("dungeonLocationID", new PyInt(offer.dungeonLocationID));
    keywords->SetItemString("dungeonSolarSystemID", new PyInt(offer.dungeonSolarSystemID));

    if (is_log_enabled(AGENT__RSP_DUMP)) {
        _log(AGENT__RSP_DUMP, "AgentBound::Handle_GetMissionKeywords() RSP:" );
        keywords->Dump(AGENT__RSP_DUMP, "    ");
    }

    return keywords;
}

PyResult AgentBound::GetMissionObjectiveInfo(PyCallArgs &call, std::optional <PyInt*> characterID, std::optional <PyInt*> contentID)
{
    // sends charID, contentID (although there's another call without any parameters, hence the optionals used)
    // returns PyDict loaded with mission info  or PyNone
    //  returning mission info sets double-pane view, where PyNone sets single-pane view
    _log(AGENT__DUMP,  "AgentBound::Handle_GetMissionObjectiveInfo() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    MissionOffer offer = MissionOffer();
    if (!m_agent->HasMission(call.client->GetCharacterID(), offer))
        return PyStatic.NewNone();

    switch (offer.stateID) {
        case Mission::State::Completed:
        case Mission::State::Rejected:
        case Mission::State::Expired: {
            return PyStatic.NewNone();
        }
    }

    return GetMissionObjectiveInfo(call.client, offer);
}

PyResult AgentBound::GetMyJournalDetails(PyCallArgs &call) {
    //parallelCalls.append((sm.GetService('agents').GetAgentMoniker(agentID).GetMyJournalDetails, ()))
    //missionState, importantMission, missionType, missionName, agentID, expirationTime, bookmarks, remoteOfferable, remoteCompletable = each
    // this is to update ONLY info with this agent....
    _log(AGENT__DUMP,  "AgentBound::Handle_GetMyJournalDetails() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    PyTuple *tuple = new PyTuple(2);
    //missions:
    PyList* missions = new PyList();
    MissionOffer offer = MissionOffer();
    if (m_agent->HasMission(call.client->GetCharacterID(), offer)) {
        int8 clientState = MissionDataMgr::GetClientState(offer.stateID);
        if (clientState > -1) {
            PyTuple* mData = new PyTuple(9);
                mData->SetItem(0, new PyInt(clientState)); //missionState
                mData->SetItem(1, new PyInt(offer.important?1:0)); //importantMission  -- integer boolean
                mData->SetItem(2, new PyString(sMissionDataMgr.GetTypeLabel(offer.typeID))); //missionTypeLabel
                mData->SetItem(3, new PyString(offer.name)); //missionName
                mData->SetItem(4, new PyInt(offer.agentID)); //agentID
                mData->SetItem(5, new PyLong(offer.expiryTime)); //expirationTime
                //bookmarks -- if populated, this is PyList of PyDicts as defined below...
                mData->SetItem(6, (offer.bookmarks == nullptr ? new PyList() : offer.bookmarks->Clone()));
                mData->SetItem(7, new PyBool(offer.remoteOfferable)); //remoteOfferable
                mData->SetItem(8, new PyBool(offer.remoteCompletable)); //remoteCompletable
            missions->AddItem(mData);
        }
    }
    tuple->SetItem(0, missions);

    //research:
    PyList* research = new PyList();
    tuple->SetItem(1, research);

    if (is_log_enabled(AGENT__RSP_DUMP))
        tuple->Dump(AGENT__RSP_DUMP, "   ");
    return tuple;
}

PyResult AgentBound::GetMissionJournalInfo(PyCallArgs &call, std::optional <PyInt*> characterID, std::optional <PyInt*> contentID) {
    //called on rclick in journal to "read details"
    //ret = self.GetAgentMoniker(agentID).GetMissionJournalInfo(charID, contentID)
    _log(AGENT__DUMP,  "AgentBound::Handle_GetMissionJournalInfo() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    MissionOffer offer = MissionOffer();
    if (!m_agent->HasMission(call.client->GetCharacterID(), offer))
        return PyStatic.NewNone();

    PyDict* journalInfo = new PyDict();
    journalInfo->SetItemString("contentID", new PyInt(offer.offerID));
    journalInfo->SetItemString("missionNameID", new PyString(offer.name));
    journalInfo->SetItemString("briefingTextID", new PyInt(offer.briefingID));
    journalInfo->SetItemString("missionState", new PyInt(MissionDataMgr::GetClientState(offer.stateID)));
    //  agents.py BuildJournalHTML() only skips the expiry line when this is None, not when it is 0.
    journalInfo->SetItemString("expirationTime", (offer.expiryTime > 0 ? (PyRep*)new PyLong(offer.expiryTime) : PyStatic.NewNone()));
    journalInfo->SetItemString("objectives", GetMissionObjectiveInfo(call.client, offer));
    //  agents.py BuildJournalHTML() indexes 'missionImage' unconditionally, so it always has to be
    //  in the dict, whatever the mission type turns out to be.
    switch(offer.typeID) {
        case Mission::Type::Mining:
            journalInfo->SetItemString("missionImage", sMissionDataMgr.GetMiningRes()); break;
        case Mission::Type::Encounter:
            journalInfo->SetItemString("missionImage", sMissionDataMgr.GetKillRes()); break;
        case Mission::Type::Courier:
        default:
            journalInfo->SetItemString("missionImage", sMissionDataMgr.GetCourierRes()); break;
    }

    if (is_log_enabled(AGENT__RSP_DUMP)) {
        _log(AGENT__RSP_DUMP, "AgentBound::Handle_GetMissionJournalInfo() RSP:" );
        journalInfo->Dump(AGENT__RSP_DUMP, "    ");
    }

    return journalInfo;
}

PyDict* AgentBound::GetMissionObjectiveInfo(Client* pClient, MissionOffer& offer)
{
    PyDict* objectiveData = new PyDict();
    objectiveData->SetItemString("missionTitleID", new PyString(offer.name));
    objectiveData->SetItemString("contentID", new PyInt(offer.offerID));
    objectiveData->SetItemString("importantStandings", new PyInt(offer.important));     // boolean integer
    // will need to test for this to set correctly.....
    if (pClient->IsMissionComplete(offer)) {     // Mission::Status:: data here 0=no, 1=yes, 2=cheat
        objectiveData->SetItemString("completionStatus", new PyInt(Mission::Status::Complete));
    } else {
        objectiveData->SetItemString("completionStatus", new PyInt(Mission::Status::Incomplete));
    }
    objectiveData->SetItemString("missionState", new PyInt(MissionDataMgr::GetClientState(offer.stateID)));   // client-facing state; drives agentGift "granted"/"will be granted" wording
    objectiveData->SetItemString("loyaltyPoints", new PyInt(offer.rewardLP));
    objectiveData->SetItemString("researchPoints", new PyInt(0));

    /*  this puts title/msg at bottom of right pane
    if (offer.stateID == Mission::State::Accepted)
        if (offer.typeID == Mission::Type::Courier) {
            PyTuple* missionExtra = new PyTuple(2);  // this is tuple(2)  headerID, bodyID    -- std locale msgIDs
                missionExtra->SetItem(0, new PyString("Reminder...."));   // this should be separate title from mission name
                missionExtra->SetItem(1, new PyString("Remember to get the %s from your hangar before you leave.", call.client->GetCourierItemRef(m_agent->GetID())->name()));   // this is additional info about mission, etc.
            objectiveData->SetItemString("missionExtra", missionExtra);
        } */

    PyList* locList = new PyList();    // tuple of list of locationIDs (pickup and dropoff)
        locList->AddItem(new PyInt(offer.originSystemID));
        locList->AddItem(new PyInt(offer.destinationSystemID));
    objectiveData->SetItemString("locations", locList);

    PyList* giftList = new PyList();    // this is list of tuple(3)  typeID, quantity, extra
    /*
    PyDict* extra = new PyDict();    // 'extra' is either specificItemID or blueprint data.
        extra->SetItemString("specificItemID", PyStatic.NewNone());
        extra->SetItemString("blueprintInfo", PyStatic.NewNone());
    PyTuple* agentGift = new PyTuple(3);
        agentGift->SetItem(0, PyStatic.NewNone());
        agentGift->SetItem(1, PyStatic.NewNone());
        agentGift->SetItem(2, extra);
        giftList->AddItem(agentGift);
    */
    objectiveData->SetItemString("agentGift", giftList);

    PyList* normList = new PyList();    // this is list of tuple(3)  typeID, quantity, extra
    if (offer.rewardISK) {
        PyDict* extra = new PyDict();    // 'extra' is either specificItemID or blueprint data.
            //extra->SetItemString("specificItemID", PyStatic.NewNone());
            //extra->SetItemString("blueprintInfo", PyStatic.NewNone());
        PyTuple* normalRewards = new PyTuple(3);
            normalRewards->SetItem(0, new PyInt(itemTypeCredits));
            normalRewards->SetItem(1, new PyInt(offer.rewardISK));
            normalRewards->SetItem(2, extra);
        normList->AddItem(normalRewards);
    }
    if (offer.rewardItemID) {
        PyDict* extra = new PyDict();    // 'extra' is either specificItemID or blueprint data.
            //extra->SetItemString("specificItemID", PyStatic.NewNone());
            //extra->SetItemString("blueprintInfo", PyStatic.NewNone());
        PyTuple* normalRewards = new PyTuple(3);
            normalRewards->SetItem(0, new PyInt(offer.rewardItemID));
            normalRewards->SetItem(1, new PyInt(offer.rewardItemQty));
            normalRewards->SetItem(2, extra);
        normList->AddItem(normalRewards);
    }
    objectiveData->SetItemString("normalRewards", normList);

    PyList* collateralList = new PyList(); // this is list of tuple(3)  typeID, quantity, extra
    /*
    PyDict* extra = new PyDict();    // 'extra' is either specificItemID or blueprint data.
        extra->SetItemString("specificItemID", PyStatic.NewNone());
        extra->SetItemString("blueprintInfo", PyStatic.NewNone());
    PyTuple* collateral = new PyTuple(3);
        collateral->SetItem(0, PyStatic.NewNone());
        collateral->SetItem(1, PyStatic.NewNone());
        collateral->SetItem(2, extra);
        */
    objectiveData->SetItemString("collateral", collateralList);

    PyList* bonusList = new PyList();   // this is list of tuple(4)  timeRemaining, typeID, quantity, extra
    if ((offer.bonusTime > 0) and (offer.bonusISK > 0)) {
        PyDict* extra = new PyDict();    // 'extra' is either specificItemID or blueprint data.
            //extra->SetItemString("specificItemID", PyStatic.NewNone());
            //extra->SetItemString("blueprintInfo", PyStatic.NewNone());
        /*  bonusTime is in minutes, but (dateAccepted - dateIssued) is already a filetime delta in
         *  100ns units - multiplying that by EvE::Time::Minute made the value hugely negative, so
         *  BuildObjectiveHTML() printed "Bonus no longer available" on every accepted mission.  The
         *  bonus clock runs from dateAccepted, matching the payout test in DoAction()'s Complete.
         */
        int64 bonusWindow = (int64)offer.bonusTime * EvE::Time::Minute;
        PyTuple* bonusRewards = new PyTuple(4);
        if (offer.dateAccepted > 0) {
            bonusRewards->SetItem(0, new PyLong(bonusWindow - (int64)(GetFileTimeNow() - offer.dateAccepted)));
        } else {
            bonusRewards->SetItem(0, new PyLong(bonusWindow));
        }
            bonusRewards->SetItem(1, new PyInt(itemTypeCredits));   // bonus is *usually* isk.  for now, we'll keep it as isk (easier)
            bonusRewards->SetItem(2, new PyInt(offer.bonusISK));    // advertise what Complete actually pays
            bonusRewards->SetItem(3, extra);
        bonusList->AddItem(bonusRewards);
    }
    // bonusList can be multiple items, usualy only item or isk for time bonus
    if (false/*bonus2*/) {
        PyDict* extra = new PyDict();    // 'extra' is either specificItemID or blueprint data.
            //extra->SetItemString("specificItemID", PyStatic.NewNone());
            //extra->SetItemString("blueprintInfo", PyStatic.NewNone());
        PyTuple* bonusRewards2 = new PyTuple(4);
            bonusRewards2->SetItem(0, new PyLong(12000000000)); //20m
            bonusRewards2->SetItem(1, new PyInt(itemTypeTrit));
            bonusRewards2->SetItem(2, new PyInt(offer.rewardISK));
            bonusRewards2->SetItem(3, extra);
        bonusList->AddItem(bonusRewards2);
    }
    objectiveData->SetItemString("bonusRewards", bonusList);
    /*  for collateral and rewards, as follows...
    typeID, quantity, extra in objectiveData['normalRewards']
    typeID, quantity, extra in objectiveData['collateral']
    typeID, quantity, extra in objectiveData['agentGift']
    or
    timeRemaining, typeID, quantity, extra in objectiveData['bonusRewards']

        specificItemID = extra.get('specificItemID', 0)
        blueprintInfo = extra.get('blueprintInfo', None)
        */

    objectiveData->SetItemString("objectives", GetMissionObjectives(pClient, offer));
    PyList* dunList = new PyList();  // this is a list of dunData dicts
    /*
    PyDict* dunData = new PyDict();
        dunData->SetItemString("dungeonID", new PyInt(1000));
        dunData->SetItemString("completionStatus", new PyInt(Dungeon::Status::Started));
        dunData->SetItemString("optional", new PyInt());
        dunData->SetItemString("briefingMessage", new PyInt());
        dunData->SetItemString("objectiveCompleted", new PyBool(false));
        dunData->SetItemString("ownerID", new PyInt(m_agent->GetID()));
        dunData->SetItemString("shipRestrictions", new PyInt(0));   // 0=normal 1=special with link to *something else*
        dunData->SetItemString("location", m_agent->GetLocationWrap());
    */
    objectiveData->SetItemString("dungeons", dunList);
    /* dunData data....
     * dungeonID
     * completionStatus
     * optional
     * briefingMessage
     * objectiveCompleted
     * ownerID
     * location
        location['locationID']
        location['locationType']
        location['solarsystemID']
        location['coords']
        location['agentID']
        ?location['referringAgentID']
        ?location['shipTypeID']
     * shipRestrictions  0=normal 1=special with link to *something else*
     */

    if (is_log_enabled(AGENT__RSP_DUMP)) {
        _log(AGENT__RSP_DUMP, "AgentBound::Handle_GetMissionObjectiveInfo() RSP:" );
        objectiveData->Dump(AGENT__RSP_DUMP, "    ");
    }

    return objectiveData;
}

PyTuple* AgentBound::GetMissionObjectives(Client* pClient, MissionOffer& offer)
{
    /*  agentDialogueUtil.LocationWrapper() indexes locationID, solarsystemID and typeID directly on
     *  every location dict it is handed, so they have to be present on both branches below.  The
     *  extra keys (coords/agentID/shipTypeID/referringAgentID) only apply to an agent sitting in
     *  space, and LocationWrapper() probes for those with 'in'.
     */
    PyDict* dropoffLocation = new PyDict();
        dropoffLocation->SetItemString("typeID", new PyInt(offer.destinationTypeID) );
        dropoffLocation->SetItemString("locationID", new PyInt(offer.destinationID) );
        dropoffLocation->SetItemString("solarsystemID", new PyInt(offer.destinationSystemID) );
    if (!sDataMgr.IsStation(offer.destinationID)) {
        dropoffLocation->SetItemString("shipTypeID", new PyInt(offer.destinationTypeID) );
        dropoffLocation->SetItemString("agentID", new PyInt(offer.destinationOwnerID) );
        // get agent in space location and set here
        PyTuple* coords = new PyTuple(3);
            coords->SetItem(0, new PyFloat(0)); //x
            coords->SetItem(1, new PyFloat(0)); //y
            coords->SetItem(2, new PyFloat(0)); //z
        dropoffLocation->SetItemString("coords", coords);
        dropoffLocation->SetItemString("referringAgentID", new PyInt(offer.agentID) );
    }

    /*  agentDialogueUtil.BuildObjectiveHTML() does "for objType, objData in objectives", so every
     *  element has to be an unpackable 2-tuple.  A fixed-size PyTuple(1) left a null slot (server
     *  fault on marshal) for any mission type the switch below does not handle, and a PyNone entry
     *  raised a TypeError client side.  Collect the entries first and size the tuple to match.
     */
    std::vector<PyTuple*> objList;
    switch (offer.typeID) {
        case Mission::Type::Trade:
        case Mission::Type::Courier: {
            PyDict* pickupLocation = new PyDict();
                pickupLocation->SetItemString("typeID", new PyInt(m_agent->GetLocTypeID()) );
                pickupLocation->SetItemString("locationID", new PyInt(offer.originID) );
                pickupLocation->SetItemString("solarsystemID", new PyInt(offer.originSystemID) );
            PyDict* cargo = new PyDict();
                cargo->SetItemString("hasCargo", new PyBool(pClient->ContainsTypeQty(offer.courierTypeID, offer.courierAmount)));
                cargo->SetItemString("typeID", new PyInt(offer.courierTypeID));
                cargo->SetItemString("quantity", new PyInt(offer.courierAmount));
                cargo->SetItemString("volume", new PyFloat(offer.courierItemVolume * offer.courierAmount));    // calculated shipment volume.  *this is direct to window*
            PyTuple* objData = new PyTuple(5);
                objData->SetItem(0, new PyInt(offer.originOwnerID));
                objData->SetItem(1, pickupLocation/*m_agent->GetLocationWrap()*/);
                objData->SetItem(2, new PyInt(offer.destinationOwnerID));
                objData->SetItem(3, dropoffLocation/*m_agent->GetLocationWrap()*/);
                objData->SetItem(4, cargo);
            PyTuple* objType = new PyTuple(2);   // this is list of tuple(2)    objType, objData
                objType->SetItem(0, new PyString("transport"));
                objType->SetItem(1, objData);
            objList.push_back(objType);
        } break;
        case Mission::Type::Encounter:
        case Mission::Type::Mining: {
            PyDict* cargo = new PyDict();
                cargo->SetItemString("hasCargo", new PyBool(pClient->ContainsTypeQty(offer.courierTypeID, offer.courierAmount)));
                cargo->SetItemString("typeID", new PyInt(offer.courierTypeID));
                cargo->SetItemString("quantity", new PyInt(offer.courierAmount));
                cargo->SetItemString("volume", new PyFloat(offer.courierItemVolume * offer.courierAmount));    // calculated shipment volume.  *this is direct to window*
            PyTuple* objData = new PyTuple(3);
                objData->SetItem(0, new PyInt(offer.destinationOwnerID));
                objData->SetItem(1, dropoffLocation/*m_agent->GetLocationWrap()*/);
                objData->SetItem(2, cargo);
            PyTuple* objType = new PyTuple(2);   // this is list of tuple(2)    objType, objData
                objType->SetItem(0, new PyString("fetch"));
                objType->SetItem(1, objData);
            objList.push_back(objType);
        } break;
        case Mission::Type::Anomic:
        case Mission::Type::EpicArc:
        case Mission::Type::Burner:
        case Mission::Type::Cosmos:
        case Mission::Type::Data:
        case Mission::Type::Research:
        case Mission::Type::Storyline:
        case Mission::Type::Tutorial:
        default: {
            /* no objective data for these yet - hand back an empty tuple, never a null slot */
        } break;
    }

    PyTuple* objectives = new PyTuple(objList.size());
    for (size_t i = 0; i < objList.size(); ++i) {
        objectives->SetItem(i, objList[i]);
        PyDecRef(objList[i]);
    }

    // cleanup
    PySafeDecRef(dropoffLocation);

    return objectives;

    /*  objectives data...
    if objType == 'agent':      -- report to agent
        agentID, agentLocation = objData
        agentLocation['locationID']
        agentLocation['locationType']
        agentLocation['solarsystemID']
        if not in station
            agentLocation['coords']
            agentLocation['agentID']
            ?agentLocation['referringAgentID']
            ?agentLocation['shipTypeID']

    elif objType == 'transport':        -- courier and trade missions
        pickupOwnerID, pickupLocation, dropoffOwnerID, dropoffLocation, cargo = objData
        pickupLocation['locationID']
        pickupLocation['locationType']
        pickupLocation['solarsystemID']
        dropoffLocation['locationID']
        dropoffLocation['locationType']
        dropoffLocation['solarsystemID']
        if not in station
            dropoffLocation['coords']
            dropoffLocation['agentID']
            ?dropoffLocation['referringAgentID']
            ?dropoffLocation['shipTypeID']
        cargo['hasCargo']
        cargo['typeID']
        cargo['volume']
        cargo['quantity']

    elif objType == 'fetch':            -- encounter and mining missions
        dropoffOwnerID, dropoffLocation, cargo = objData
        dropoffLocation['locationID']
        dropoffLocation['locationType']
        dropoffLocation['solarsystemID']
        if not in station
            dropoffLocation['coords']
            dropoffLocation['agentID']
            ?dropoffLocation['referringAgentID']
            ?dropoffLocation['shipTypeID']
        cargo['hasCargo']
        cargo['typeID']
        cargo['volume']
        cargo['quantity']
        */
}


/**     ***********************************************************************
 * @note   these do absolutely nothing at this time....
 */

PyResult AgentBound::GetDungeonShipRestrictions(PyCallArgs &call, PyInt* dungeonID) {
    //restrictions = self.GetAgentMoniker(agentID).GetDungeonShipRestrictions(dungeonID)
    _log(AGENT__DUMP,  "AgentBound::Handle_GetDungeonShipRestrictions() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    /*  No dungeon restrictions are modelled yet, but agents.py PopupDungeonShipRestrictionList()
     *  reads .allowedShipTypes / .restrictedShipTypes off whatever comes back without a None check,
     *  so hand it an object with both lists empty rather than PyNone.
     */
    PyDict* args = new PyDict();
        args->SetItemString("allowedShipTypes", new PyList());
        args->SetItemString("restrictedShipTypes", new PyList());

    return new PyObject("util.KeyVal", args);
}

PyResult AgentBound::RemoveOfferFromJournal(PyCallArgs &call) {
    //called on rclick in journal to "remove offer"
    //self.GetAgentMoniker(agentID).RemoveOfferFromJournal()
    _log(AGENT__DUMP,  "AgentBound::Handle_RemoveOfferFromJournal() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    //  missionentry.py only offers this menu item on an un-accepted offer, so dropping it costs the
    //  player nothing but the offer itself.
    MissionOffer offer = MissionOffer();
    if (!m_agent->HasMission(call.client->GetCharacterID(), offer))
        return PyStatic.NewNone();

    if (offer.stateID > Mission::State::Offered) {
        _log(AGENT__WARNING, "AgentBound::Handle_RemoveOfferFromJournal() - offer %u for char %u is state %u; not removing.",
             offer.offerID, call.client->GetCharacterID(), offer.stateID);
        return PyStatic.NewNone();
    }

    m_agent->DeleteOffer(call.client->GetCharacterID());
    m_agent->SendMissionUpdate(call.client, "offer_removed");

    return PyStatic.NewNone();
}

PyResult AgentBound::GetOfferJournalInfo(PyCallArgs &call) {
    //html = self.GetAgentMoniker(agentID).GetOfferJournalInfo()
    _log(AGENT__DUMP,  "AgentBound::Handle_GetOfferJournalInfo() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    return nullptr;
}

PyResult AgentBound::GetEntryPoint(PyCallArgs &call) {
    //entryPoint = sm.StartService('agents').GetAgentMoniker(bookmark.agentID).GetEntryPoint()
    _log(AGENT__DUMP,  "AgentBound::Handle_GetEntryPoint() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    return nullptr;
}

PyResult AgentBound::GotoLocation(PyCallArgs &call, PyString* locationType, PyInt* locationNumber, std::optional <PyInt*> referringAgentID) {
    //sm.StartService('agents').GetAgentMoniker(bookmark.agentID).GotoLocation(bookmark.locationType, bookmark.locationNumber, referringAgentID)
    _log(AGENT__DUMP,  "AgentBound::Handle_GotoLocation() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    return nullptr;
}

PyResult AgentBound::WarpToLocation(PyCallArgs &call, PyString* locationType, PyInt* locationNumber, PyFloat* warpRange, PyBool* fleet, std::optional <PyInt*> referringAgentID) {
    //sm.StartService('agents').GetAgentMoniker(bookmark.agentID).WarpToLocation(bookmark.locationType, bookmark.locationNumber, warpRange, fleet, referringAgentID)
    _log(AGENT__DUMP,  "AgentBound::Handle_WarpToLocation() - size=%lli", call.tuple->size());
    call.Dump(AGENT__DUMP);

    return nullptr;
}
