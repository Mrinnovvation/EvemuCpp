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
    Author:        Aknor Jaden
*/

#include "eve-server.h"

#include "apiserver/APICharacterManager.h"
#include "inventory/AttributeEnum.h"

APICharacterManager::APICharacterManager()
: APIServiceManager()
{
}

/* Maps a skill's primary/secondary attribute *ID* (as text, e.g. "165") onto the
 * character's current value for that attribute. Returns false when the character
 * has no such attribute, rather than dereferencing map::end().
 */
/* Reads one attribute out of the map APICharacterDB fills in, keyed by the attribute
 * ID rendered as text. Returns "0" when the character has no row for that attribute
 * rather than dereferencing map::end().
 */
std::string APICharacterManager::_AttributeText(const std::map<std::string, std::string> & attributes, uint32 attributeID)
{
    std::map<std::string, std::string>::const_iterator itr = attributes.find( std::string(itoa(attributeID)) );
    return ((itr == attributes.end()) ? std::string("0") : itr->second);
}

bool APICharacterManager::_LookupLearningAttribute(const std::map<uint32, uint32> & attributes,
                                                   const std::string & attributeIDText, uint32 * value)
{
    std::map<uint32, uint32>::const_iterator itr = attributes.find( atoi( attributeIDText.c_str() ) );
    if ( itr == attributes.end() )
        return false;

    *value = itr->second;
    return true;
}

std::shared_ptr<std::string> APICharacterManager::ProcessCall(const APICommandCall * pAPICommandCall)
{
    sLog.Debug("APIAdminManager::ProcessCall()", "EVEmu API - Character Service Manager");

    if ( pAPICommandCall->find( "servicehandler" ) == pAPICommandCall->end() )
    {
        sLog.Error( "APICharacterManager::ProcessCall()", "Cannot find 'servicehandler' specifier in pAPICommandCall packet" );
        return std::shared_ptr<std::string>(new std::string(""));
    }

    if ( pAPICommandCall->find( "servicehandler" )->second == "CharacterSheet.xml.aspx" )
        return _CharacterSheet(pAPICommandCall);
    if ( pAPICommandCall->find( "servicehandler" )->second == "SkillQueue.xml.aspx" )
        return _SkillQueue(pAPICommandCall);
    if ( pAPICommandCall->find( "servicehandler" )->second == "SkillInTraining.xml.aspx" )
        return _SkillInTraining(pAPICommandCall);
    //else if ( pAPICommandCall->find( "servicehandler" )->second == "TODO.xml.aspx" )
    //    return _TODO(pAPICommandCall);
    else
    {
        sLog.Error("APIAdminManager::ProcessCall()", "EVEmu API - Admin Service Manager - ERROR: Cannot resolve '%s' as a valid service query for Admin Service Manager",
            pAPICommandCall->find("servicehandler")->second.c_str() );
        return std::shared_ptr<std::string>(new std::string(""));
    }
    sLog.Debug("APICharacterManager::ProcessCall()", "EVEmu API - Character Service Manager");

    return BuildErrorXMLResponse( "9999", "EVEmu API Server: Character Manager - Unknown call." );
}

std::shared_ptr<std::string> APICharacterManager::_CharacterSheet(const APICommandCall * pAPICommandCall)
{
    size_t i;

    sLog.Debug("APICharacterManager::_CharacterSheet()", "EVEmu API - Character Service Manager - CALL: CharacterSheet.xml.aspx");

    uint32 characterID = 0;
    std::shared_ptr<std::string> authError = _ValidateCharacterAccess( pAPICommandCall, EVEAPI::KeyType::Limited, &characterID );
    if ( authError )
        return authError;

    // Make calls to the APICharacterDB class to grab all data for this call and populate the xml structure with that data
    std::vector<std::string> skillTypeIDList;
    std::vector<std::string> skillPointsList;
    std::vector<std::string> skillLevelList;
    std::vector<std::string> skillPublishedList;
    m_charDB.GetCharacterSkillsTrained( characterID, skillTypeIDList, skillPointsList, skillLevelList, skillPublishedList );

    std::vector<std::string> charInfoList;
    std::map<std::string, std::string> charAttributes;

    /* The XML below indexes charInfoList positionally, so a failed lookup has to stop
     * the call here rather than reaching .at() on an empty vector.
     */
    if ( !m_charDB.GetCharacterInfo( characterID, charInfoList ) or (charInfoList.size() < 15) )
    {
        sLog.Error( "APICharacterManager::_CharacterSheet()", "Character info lookup failed for characterID %u", characterID );
        return BuildErrorXMLResponse( "105", "Invalid characterID." );
    }

    m_charDB.GetCharacterAttributes( characterID, charAttributes );

    sLog.Error( "APICharacterManager::_CharacterSheet()", "INFO: Sections Currently hard-coded: attributeEnhancers, certificates, corporation roles" );

    std::vector<std::string> rowset;
    _BuildXMLHeader();
    {
        _BuildXMLTag( "result" );
        {
            _BuildSingleXMLTag( "characterID", std::string(itoa(characterID)) );
            _BuildSingleXMLTag( "name", charInfoList.at(12) );
            _BuildSingleXMLTag( "DoB", Win32TimeToString(strtoull(charInfoList.at(8).c_str(), NULL, 0) ));
            _BuildSingleXMLTag( "race", charInfoList.at(11) );
            _BuildSingleXMLTag( "bloodLine", charInfoList.at(10) );
            _BuildSingleXMLTag( "ancestry", charInfoList.at(9) );
            _BuildSingleXMLTag( "gender", ( (atoi(charInfoList.at(14).c_str()) > 0) ? "Male" : "Female") );
            _BuildSingleXMLTag( "corporationName", charInfoList.at(13) );
            _BuildSingleXMLTag( "corporationID", charInfoList.at(2) );
            _BuildSingleXMLTag( "allianceName", "none" );
            _BuildSingleXMLTag( "allianceID", "0" );
            _BuildSingleXMLTag( "cloneName", "Clone Grade Pi" );
            _BuildSingleXMLTag( "cloneSkillPoints", "54600000" );
            _BuildSingleXMLTag( "balance", charInfoList.at(0) );

            // TODO: code these for real, as what follows are hard coded examples to fill out the whole xml document:
            // Attribute Enhancers (implants)
            _BuildXMLTag( "attributeEnhancers" );
            {
                /*
                _BuildXMLTag( "memoryBonus" );
                {
                    _BuildSingleXMLTag( "augmentatorName", "Memory Augmentation - Basic" );
                    _BuildSingleXMLTag( "augmentatorValue", "3" );
                }
                _CloseXMLTag(); // close tag "memoryBonus"
                _BuildXMLTag( "perceptionBonus" );
                {
                    _BuildSingleXMLTag( "augmentatorName", "Ocular Filter - Basic" );
                    _BuildSingleXMLTag( "augmentatorValue", "3" );
                }
                _CloseXMLTag(); // close tag "perceptionBonus"
                _BuildXMLTag( "willpowerBonus" );
                {
                    _BuildSingleXMLTag( "augmentatorName", "Neural Boost - Basic" );
                    _BuildSingleXMLTag( "augmentatorValue", "3" );
                }
                _CloseXMLTag(); // close tag "willpowerBonus"
                _BuildXMLTag( "intelligenceBonus" );
                {
                    _BuildSingleXMLTag( "augmentatorName", "Snake Delta" );
                    _BuildSingleXMLTag( "augmentatorValue", "3" );
                }
                _CloseXMLTag(); // close tag "intelligenceBonus"
                _BuildXMLTag( "charismaBonus" );
                {
                    _BuildSingleXMLTag( "augmentatorName", "Limited Social Adaptation Chip" );
                    _BuildSingleXMLTag( "augmentatorValue", "3" );
                }
                _CloseXMLTag(); // close tag "charismaBonus"
                */
            }
            _CloseXMLTag(); // close tag "attributeEnhancers"

            // TODO: code these for real, as what follows are hard coded examples to fill out the whole xml document:
            // Attributes
            _BuildXMLTag( "attributes" );
            {
                _BuildSingleXMLTag( "intelligence", _AttributeText( charAttributes, AttrIntelligence ) );
                _BuildSingleXMLTag( "memory",       _AttributeText( charAttributes, AttrMemory ) );
                _BuildSingleXMLTag( "charisma",     _AttributeText( charAttributes, AttrCharisma ) );
                _BuildSingleXMLTag( "perception",   _AttributeText( charAttributes, AttrPerception ) );
                _BuildSingleXMLTag( "willpower",    _AttributeText( charAttributes, AttrWillpower ) );
            }
            _CloseXMLTag(); // close tag "attributes"

            // Skills
            rowset.clear();
            rowset.push_back("typeID");
            rowset.push_back("skillpoints");
            rowset.push_back("level");
            rowset.push_back("published");
            _BuildXMLRowSet( "skills", "typeID", &rowset );
            {
                for(i=0; i<skillTypeIDList.size(); i++)
                {
                    rowset.clear();
                    rowset.push_back(skillTypeIDList.at(i));
                    rowset.push_back(skillPointsList.at(i));
                    rowset.push_back(skillLevelList.at(i));
                    rowset.push_back(skillPublishedList.at(i));
                    _BuildXMLRow( &rowset );
                }
            }
            _CloseXMLRowSet();  // close rowset "skills"

            // TODO: code these for real, as what follows are hard coded examples to fill out the whole xml document:
            // Certificates:
            rowset.clear();
            rowset.push_back("certificateID");
            _BuildXMLRowSet( "certificates", "certificateID", &rowset );
            {
            //    for(i=0; i<skillTypeIDList.size(); i++)
            //    {
                    rowset.clear();
                    //rowset.push_back(certificatesList.at(i));
                    rowset.push_back("1");
                    _BuildXMLRow( &rowset );
            //    }
            }
            _CloseXMLRowSet();  // close rowset "certificates"

            // TODO: code these for real, as what follows are hard coded examples to fill out the whole xml document:
            // Corporation Roles:
            //rowset.clear();
            //rowset.push_back("");
            //_BuildXMLRowSet( "", "", &rowset );
            //{
            //}
            _CloseXMLRowSet();  // close rowset ""
        }
        _CloseXMLTag(); // close tag "result"
    }
    _CloseXMLHeader( EVEAPI::CacheStyles::Long );

    return _GetXMLDocumentString();
}

std::shared_ptr<std::string> APICharacterManager::_SkillQueue(const APICommandCall * pAPICommandCall)
{
    size_t i;

    sLog.Debug("APICharacterManager::_SkillQueue()", "EVEmu API - Character Service Manager - CALL: SkillQueue.xml.aspx");

    uint32 characterID = 0;
    std::shared_ptr<std::string> authError = _ValidateCharacterAccess( pAPICommandCall, EVEAPI::KeyType::Limited, &characterID );
    if ( authError )
        return authError;

    // Make calls to the APICharacterDB class to grab all data for this call and populate the xml structure with that data

    std::map<std::string, std::string> charLearningAttributesString;
    std::map<uint32, uint32> charLearningAttributes;
    m_charDB.GetCharacterAttributes( characterID, charLearningAttributesString );
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrMemory, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrMemory ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrIntelligence, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrIntelligence ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrCharisma, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrCharisma ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrWillpower, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrWillpower ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrPerception, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrPerception ).c_str() ) ));

    std::vector<std::string> queueOrderList;
    std::vector<std::string> queueSkillTypeIdList;
    std::vector<std::string> queueSkillLevelList;
    std::vector<std::string> queueSkillRankList;
    std::vector<std::string> queueSkillIdList;
    std::vector<std::string> queueSkillPrimaryAttrList;
    std::vector<std::string> queueSkillSecondaryAttrList;
    std::vector<std::string> queueSkillPointsTrainedList;

    uint32 queueSkillPrimaryAttribute = 0;
    uint32 queueSkillSecondaryAttribute = 0;
    std::vector<uint32> queueSkillStartSP;
    std::vector<uint32> queueSkillEndSP;
    std::vector<int64> queueSkillStartTime;
    std::vector<int64> queueSkillEndTime;
    uint8 spPerMinute = 0;
    uint32 skillStartSP = 0;
    uint32 skillEndSP = 0;
    int64 timeNow = 0;
    int64 skillStartTime = 0;
    int64 skillEndTime = 0;

    bool status = m_charDB.GetCharacterSkillQueue( characterID, queueOrderList, queueSkillTypeIdList, queueSkillLevelList, queueSkillRankList,
        queueSkillIdList, queueSkillPrimaryAttrList, queueSkillSecondaryAttrList, queueSkillPointsTrainedList );

    sLog.Error( "APICharacterManager::_SkillQueue()", "INFO: Calculation of Skill End Time based on Effective SP/min does NOT include implants/boosters at this time" );

    if ( status )
    {
        timeNow = static_cast<int64>( GetFileTimeNow() );
        for( i=0; i<queueOrderList.size(); i++ )
        {
            if ( !_LookupLearningAttribute( charLearningAttributes, queueSkillPrimaryAttrList.at(i), &queueSkillPrimaryAttribute )
              || !_LookupLearningAttribute( charLearningAttributes, queueSkillSecondaryAttrList.at(i), &queueSkillSecondaryAttribute ) )
            {
                sLog.Error( "APICharacterManager::_SkillQueue()", "Unknown learning attribute for queue entry %u of character %u", i, characterID );
                break;
            }

            skillStartSP = static_cast<uint32>( atoi( queueSkillPointsTrainedList.at(i).c_str() ) );
            queueSkillStartSP.push_back( skillStartSP );
            skillEndSP = EvEMath::Skill::PointsAtLevel( atoi(queueSkillLevelList.at(i).c_str()),
                                                        (float)atof(queueSkillRankList.at(i).c_str()) );
            queueSkillEndSP.push_back( skillEndSP );
            spPerMinute = EvEMath::Skill::PointsPerMinute( queueSkillPrimaryAttribute, queueSkillSecondaryAttribute );
            skillStartTime = EvEMath::Skill::StartTime( skillStartSP, skillEndSP, spPerMinute, timeNow );
            skillEndTime = EvEMath::Skill::EndTime( skillStartSP, skillEndSP, spPerMinute, timeNow );
            queueSkillStartTime.push_back( skillStartTime );
            queueSkillEndTime.push_back( skillEndTime );
            timeNow = skillEndTime;
        }
    }

    // EXAMPLE:
    std::vector<std::string> rowset;
    _BuildXMLHeader();
    {
        _BuildXMLTag( "result" );
        {
            // Skill Queue
            rowset.push_back("queuePosition");
            rowset.push_back("typeID");
            rowset.push_back("level");
            rowset.push_back("startSP");
            rowset.push_back("endSP");
            rowset.push_back("startTime");
            rowset.push_back("endTime");
            _BuildXMLRowSet( "skillqueue", "queuePosition", &rowset );
            {
                if ( status )
                {
                    for(i=0; i<queueOrderList.size(); i++)
                    {
                        rowset.clear();
                        rowset.push_back( queueOrderList.at(i) );
                        rowset.push_back( queueSkillTypeIdList.at(i) );
                        rowset.push_back( queueSkillLevelList.at(i) );
                        rowset.push_back( std::string(itoa(queueSkillStartSP.at(i))) );
                        rowset.push_back( std::string(itoa(queueSkillEndSP.at(i))) );
                        rowset.push_back( Win32TimeToString(queueSkillStartTime.at(i)) );
                        rowset.push_back( Win32TimeToString(queueSkillEndTime.at(i)) );
                        _BuildXMLRow( &rowset );
                    }
                }
            }
            _CloseXMLRowSet();  // close rowset "skillqueue"
        }
        _CloseXMLTag(); // close tag "result"
    }
    _CloseXMLHeader( EVEAPI::CacheStyles::Modified );

    return _GetXMLDocumentString();
}

std::shared_ptr<std::string> APICharacterManager::_SkillInTraining(const APICommandCall * pAPICommandCall)
{
    sLog.Debug("APICharacterManager::_SkillInTraining()", "EVEmu API - Character Service Manager - CALL: SkillInTraining.xml.aspx");

    uint32 characterID = 0;
    std::shared_ptr<std::string> authError = _ValidateCharacterAccess( pAPICommandCall, EVEAPI::KeyType::Limited, &characterID );
    if ( authError )
        return authError;

    // Make calls to the APICharacterDB class to grab all data for this call and populate the xml structure with that data

    std::map<std::string, std::string> charLearningAttributesString;
    std::map<uint32, uint32> charLearningAttributes;
    m_charDB.GetCharacterAttributes( characterID, charLearningAttributesString );
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrMemory, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrMemory ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrIntelligence, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrIntelligence ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrCharisma, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrCharisma ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrWillpower, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrWillpower ).c_str() ) ));
    charLearningAttributes.insert( std::pair<uint32, uint32>( AttrPerception, (uint32)atoi( _AttributeText( charLearningAttributesString, AttrPerception ).c_str() ) ));

    std::vector<std::string> queueOrderList;
    std::vector<std::string> queueSkillTypeIdList;
    std::vector<std::string> queueSkillLevelList;
    std::vector<std::string> queueSkillRankList;
    std::vector<std::string> queueSkillIdList;
    std::vector<std::string> queueSkillPrimaryAttrList;
    std::vector<std::string> queueSkillSecondaryAttrList;
    std::vector<std::string> queueSkillPointsTrainedList;

    uint32 queueSkillPrimaryAttribute = 0;
    uint32 queueSkillSecondaryAttribute = 0;
    std::vector<uint32> queueSkillStartSP;
    std::vector<uint32> queueSkillEndSP;
    std::vector<int64> queueSkillStartTime;
    std::vector<int64> queueSkillEndTime;
    uint8 spPerMinute = 0;
    uint32 skillStartSP = 0;
    uint32 skillEndSP = 0;
    int64 timeNow = 0;
    int64 skillStartTime = 0;
    int64 skillEndTime = 0;

    bool status = m_charDB.GetCharacterSkillQueue( characterID, queueOrderList, queueSkillTypeIdList, queueSkillLevelList, queueSkillRankList,
        queueSkillIdList, queueSkillPrimaryAttrList, queueSkillSecondaryAttrList, queueSkillPointsTrainedList );

    if ( status )
    {
        sLog.Error( "APICharacterManager::_SkillInTraining()", "INFO: Calculation of Skill End Time based on Effective SP/min does NOT include implants/boosters at this time" );
        timeNow = static_cast<int64>( GetFileTimeNow() );

        // An empty queue still reports status==true; .at(0) would throw and kill the API thread.
        if ( queueOrderList.empty() )
        {
            status = false;
        }
        else if ( !_LookupLearningAttribute( charLearningAttributes, queueSkillPrimaryAttrList.at(0), &queueSkillPrimaryAttribute )
               || !_LookupLearningAttribute( charLearningAttributes, queueSkillSecondaryAttrList.at(0), &queueSkillSecondaryAttribute ) )
        {
            sLog.Error( "APICharacterManager::_SkillInTraining()", "Unknown learning attribute for character %u", characterID );
            status = false;
        }
        else
        {
            skillStartSP = static_cast<uint32>( atoi( queueSkillPointsTrainedList.at(0).c_str() ) );
            queueSkillStartSP.push_back( skillStartSP );
            skillEndSP = EvEMath::Skill::PointsAtLevel( atoi(queueSkillLevelList.at(0).c_str()),
                                                        (float)atof(queueSkillRankList.at(0).c_str()) );
            queueSkillEndSP.push_back( skillEndSP );
            spPerMinute = EvEMath::Skill::PointsPerMinute( queueSkillPrimaryAttribute, queueSkillSecondaryAttribute );
            skillStartTime = EvEMath::Skill::StartTime( skillStartSP, skillEndSP, spPerMinute, timeNow );
            skillEndTime = EvEMath::Skill::EndTime( skillStartSP, skillEndSP, spPerMinute, timeNow );
            queueSkillStartTime.push_back( skillStartTime );
            queueSkillEndTime.push_back( skillEndTime );
        }
    }

    // EXAMPLE:
    std::vector<std::string> rowset;
    _BuildXMLHeader();
    {
        _BuildXMLTag( "result" );
        {
            if ( status )
            {
                _BuildSingleXMLTag( "currentTQTime", Win32TimeToString(timeNow) );
                _BuildSingleXMLTag( "trainingEndTime", Win32TimeToString(skillEndTime) );
                _BuildSingleXMLTag( "trainingStartTime", Win32TimeToString(skillStartTime) );
                _BuildSingleXMLTag( "trainingTypeID", queueSkillTypeIdList.at(0) );
                _BuildSingleXMLTag( "trainingStartSP", std::string(itoa(skillStartSP)) );
                _BuildSingleXMLTag( "trainingDestinationSP", std::string(itoa(skillEndSP)) );
                _BuildSingleXMLTag( "trainingToLevel", queueSkillLevelList.at(0) );
                _BuildSingleXMLTag( "skillInTraining", "1" );
            }
            else
            {
                _BuildSingleXMLTag( "skillInTraining", "0" );
            }
        }
        _CloseXMLTag(); // close tag "result"
    }
    _CloseXMLHeader( EVEAPI::CacheStyles::Modified );

    return _GetXMLDocumentString();
}
