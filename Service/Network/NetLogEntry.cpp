#include "pch.h"
#include "NetLogEntry.h"
#include "Firewall/WindowsFirewall.h"
#include "Firewall/WindowsFwLog.h"
#include "../../Library/API/PrivacyAPI.h"

CNetLogEntry::CNetLogEntry(const struct SWinFwLogEvent* pEvent, EFwEventStates State, const CHostNamePtr& pRemoteHostName, const std::vector<CFlexGuid>& AllowRules, const std::vector<CFlexGuid>& BlockRules, uint64 PID, const std::wstring& ServiceTag, const std::wstring& AppSid)
	: CTraceLogEntry(PID)
{
	m_State			= State;
	m_pRemoteHostName = pRemoteHostName;
	m_ServiceTag	= ServiceTag;
	m_AppSid		= AppSid;

	m_Action		= pEvent->Type;
	m_Direction		= pEvent->Direction;
	m_ProtocolType	= pEvent->ProtocolType;
	m_LocalAddress	= pEvent->LocalAddress;
	m_LocalPort		= pEvent->LocalPort;
	m_RemoteAddress	= pEvent->RemoteAddress;
	m_RemotePort	= pEvent->RemotePort;
	m_TimeStamp		= pEvent->TimeStamp;

	// todo: m_Realm

	m_AllowRules	= AllowRules;
	m_BlockRules	= BlockRules;
}

void CNetLogEntry::WriteVariant(CVariant& Entry) const
{
	CTraceLogEntry::WriteVariant(Entry);

	Entry.Write(API_V_FW_EVENT_STATE, (uint32)m_State);

	Entry.Write(API_V_FW_RULE_ACTION, (uint32)m_Action);
	Entry.Write(API_V_FW_RULE_DIRECTION, (uint32)m_Direction);
	
	Entry.Write(API_V_FW_RULE_PROTOCOL, (uint32)m_ProtocolType);
	Entry.Write(API_V_FW_RULE_LOCAL_ADDR, m_LocalAddress.ToString());
	Entry.Write(API_V_FW_RULE_LOCAL_PORT, m_LocalPort);
	Entry.Write(API_V_FW_RULE_REMOTE_ADDR, m_RemoteAddress.ToString());
	Entry.Write(API_V_FW_RULE_REMOTE_PORT, m_RemotePort);
	Entry.Write(API_V_FW_RULE_REMOTE_HOST, m_pRemoteHostName ? m_pRemoteHostName->ToString() : L"");

    //m_Realm // todo

	Entry.WriteVariant(API_V_FW_ALLOW_RULES, CFlexGuid::WriteList(m_AllowRules, false));
	Entry.WriteVariant(API_V_FW_BLOCK_RULES, CFlexGuid::WriteList(m_BlockRules, false));
}