/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        Reve
*/

//work in progress

#include "eve-server.h"


#include "dungeon/DungeonExplorationMgrService.h"

DungeonExplorationMgrService::DungeonExplorationMgrService() :
    Service("dungeonExplorationMgr")
{
    this->Add("GetMyEscalatingPathDetails", &DungeonExplorationMgrService::GetMyEscalatingPathDetails);
    this->Add("DeleteExpiredPathStep", &DungeonExplorationMgrService::DeleteExpiredPathStep);
}

PyResult DungeonExplorationMgrService::GetMyEscalatingPathDetails(PyCallArgs &call) {
    // cached response
    /**00:51:32 L DungeonExplorationMgrService::Handle_GetMyEscalatingPathDetails(): size= 0
     * 00:51:32 [SvcCall]   Call Arguments:
     * 00:51:32 [SvcCall]       Tuple: Empty
     * 00:51:32 [SvcCall]   Call Named Arguments:
     * 00:51:32 [SvcCall]     Argument 'machoVersion':
     * 00:51:32 [SvcCall]         Integer field: 1
     *
     * /client/script/ui/shared/neocom/journal.py(922) Load
     * /client/script/ui/shared/neocom/journal.py(1502) ShowExpeditions
     *        expeditionRowset = None
     *        self = form.Journal object at 0x2ebcc490, name=journal, destroyed=False>
     *        scrolllist = []
     *        reload = 0
     * TypeError: 'NoneType' object is not iterable
     *
     *
     *    def ShowExpeditions(self, reload = 0):
     *        expeditionRowset = sm.GetService('journal').GetExpeditions(force=reload)
     *        self.ShowLoad()
     *        try:
     *            scrolllist = []
     *            for expeditionData in expeditionRowset:
     *                dungeonData = expeditionData.dungeon
     *                destDungeonData = expeditionData.destDungeon
     *                pathStep = expeditionData.pathStep
     *                expiryTime = expeditionData.expiryTime
     *                if destDungeonData is not None:
     *                    jumps = sm.GetService('starmap').ShortestPath(dungeonData.solarSystemID, eve.session.solarsystemid2)
     *                    jumps = jumps or []
     *                    totaljumps = max(0, len(jumps) - 1)
     *                    if dungeonData.solarSystemID is not None:
     *                        solName = cfg.evelocations.Get(dungeonData.solarSystemID).name
     *                    else:
     *                        solName = localization.GetByLabel('UI/Common/Unknown')
     *                    txt = localization.GetByMessageID(pathStep.journalEntryTemplateID)
     *                    if len(txt) > MAX_COL_LENGTH:
     *                        txt = txt[:MAX_COL_LENGTH] + '...'
     *                    text = '<t>'.join(['      ',
     *                     util.FmtDate(dungeonData.creationTime),
     *                     solName,
     *                     str(totaljumps),
     *                     txt])
     *                    expired = not expiryTime - blue.os.GetWallclockTime() > 0
     *                    data = {'id': dungeonData.instanceID,
     *                     'rec': expeditionData,
     *                     'text': txt,
     *                     'label': text,
     *                     'hint': localization.GetByMessageID(pathStep.journalEntryTemplateID),
     *                     'jumps': totaljumps,
     *                     'expired': expired,
     *                     'sort_' + localization.GetByLabel('UI/Journal/JournalWindow/Expeditions/HeaderExpiration'): expiryTime}
     *                    scrolllist.append(listentry.Get('EscalatingPathLocationEntry', data))
     *
     */
    _log(SERVICE__CALLS, "DungeonExplorationMgrService::Handle_GetMyEscalatingPathDetails() - size=%lli", call.tuple->size());
    call.Dump(SERVICE__CALL_DUMP);

    /*  Escalating paths (expeditions) are not generated server-side yet, but the client iterates
     *  whatever this call returns the moment the journal's Expeditions tab is drawn.  Returning
     *  PyNone raises "TypeError: 'NoneType' object is not iterable" in journal.py ShowExpeditions()
     *  and takes the whole journal window down with it.  Hand back a correctly-shaped, empty
     *  util.Rowset instead.
     *    - form.Journal::ShowExpeditions() iterates the rowset, reading .dungeon, .destDungeon,
     *      .pathStep and .expiryTime from each row.
     *    - JournalSvc::OnEscalatingPathMessage() builds util.IndexRowset(rs.header, rs.lines,
     *      'instanceID'), so 'instanceID' has to be in the header too.
     */
    return GetEmptyPathRowset();
}

PyObject* DungeonExplorationMgrService::GetEmptyPathRowset()
{
    /*  util.Row raises AttributeError for anything missing from the header, and
     *  util.IndexRowset() does header.index('instanceID') even on an empty rowset, so the full
     *  column set has to be declared here even though we never send a line.
     */
    PyList* header = new PyList(6);
        header->SetItemString(0, "instanceID");
        header->SetItemString(1, "dungeon");
        header->SetItemString(2, "srcDungeon");
        header->SetItemString(3, "destDungeon");
        header->SetItemString(4, "pathStep");
        header->SetItemString(5, "expiryTime");

    PyDict* args = new PyDict();
        args->SetItemString("header", header);
        args->SetItemString("RowClass", new PyToken("util.Row"));
        args->SetItemString("lines", new PyList());

    return new PyObject("util.Rowset", args);
}

//  instanceID is taken as PyRep* rather than PyInt*: dungeon instance ids come off the item id
//  sequence and marshal as PyLong once they pass INT32_MAX, which a PyInt* parameter would reject
//  in Callable's validateArg() and turn into a method_not_found on the client.
PyResult DungeonExplorationMgrService::DeleteExpiredPathStep(PyCallArgs& call, PyRep* instanceID)
{
    /*  called from journal.py DeleteExpeditionEntry() when the player clears an expired expedition
     *  from the journal.  We have no paths to delete yet, but the call still has to answer.
     */
    _log(SERVICE__CALLS, "DungeonExplorationMgrService::Handle_DeleteExpiredPathStep() - instanceID=%li", PyRep::IntegerValue(instanceID));
    call.Dump(SERVICE__CALL_DUMP);

    return PyStatic.NewNone();
}


/**
  dungeonTracking.GetEscalatingPathDungeonsEntered()
  dungeonTracking.GetDistributionDungeonsEntered()
        (OnDistributionDungeonEntered)
sm.RemoteSvc('dungeonExplorationMgr').DeleteExpiredPathStep


*/
