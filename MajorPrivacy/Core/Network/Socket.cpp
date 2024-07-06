#include "pch.h"
#include "Socket.h"
#include "../Library/API/PrivacyAPI.h"

CSocket::CSocket(QObject* parent)
	: QObject(parent)
{
}

void CSocket::FromVariant(const class XVariant& Socket)
{
	Socket.ReadRawIMap([&](uint32 Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		switch (Index)
		{
		case API_V_SOCK_REF:		m_SocketRef = Data; break;

		case API_V_SOCK_TYPE:		m_ProtocolType = Data; break;
		case API_V_SOCK_LADDR:		m_LocalAddress = Data.AsQStr(); break;
		case API_V_SOCK_LPORT:		m_LocalPort = Data; break;
		case API_V_SOCK_RADDR:		m_RemoteAddress = Data.AsQStr(); break;
		case API_V_SOCK_RPORT:		m_RemotePort = Data; break;
		case API_V_SOCK_STATE:		m_State = Data; break;
		case API_V_SOCK_LSCOPE:		m_LocalScopeId = Data; break; // Ipv6
		case API_V_SOCK_RSCOPE:		m_RemoteScopeId = Data; break; // Ipv6

		case API_V_SOCK_PID:		m_ProcessId = Data; break;
		case API_V_SOCK_SVC_TAG:	m_OwnerService = Data.AsQStr(); break;

		case API_V_SOCK_RHOST:		m_RemoteHostName = Data.AsQStr(); break;

		case API_V_SOCK_CREATED:	m_CreateTimeStamp = Data; break;

		case API_V_SOCK_LAST_ACT:	m_LastActivity = Data; break;

		case API_V_SOCK_UPLOAD:		m_UploadRate = Data; break;
		case API_V_SOCK_DOWNLOAD:	m_DownloadRate = Data; break;
		case API_V_SOCK_UPLOADED:	m_UploadTotal = Data; break;
		case API_V_SOCK_DOWNLOADED:	m_DownloadTotal = Data; break;
		}
	});
}