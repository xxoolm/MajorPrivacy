#include "pch.h"
#include "PrivacyCore.h"
#include "Processes/ProcessList.h"
#include "Enclaves/EnclaveManager.h"
#include "Processes/ExecLogEntry.h"
#include "Programs/ProgramManager.h"
#include "Network/NetworkManager.h"
#include "Network/NetLogEntry.h"
#include "Volumes/VolumeManager.h"
#include "Tweaks/TweakManager.h"
#include "../Library/Common/Exception.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "Access/ResLogEntry.h"
#include "Access/AccessManager.h"
#include "../Library/Crypto/HashFunction.h"
#include "Helpers/WinHelper.h"

#include <phnt_windows.h>
#include <phnt.h>

#include "../Library/Helpers/AppUtil.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/Service.h"
#include "../Library/Helpers/NtPathMgr.h"

#include "../Library/Crypto/Encryption.h"
#include "../Library/Crypto/PrivateKey.h"
#include "../Library/Crypto/PublicKey.h"

CSettings* theConf = NULL;
CPrivacyCore* theCore = NULL;

#ifdef _DEBUG
void XVariantTest();
#endif

CPrivacyCore::CPrivacyCore(QObject* parent)
: QObject(parent), m_Driver(CDriverAPI::eDevice)
{
#ifdef _DEBUG
	XVariantTest();

	/*NTSTATUS Status;
	CBuffer Text1((void*)"Hello, World 123Hello, World", 28, true);
	CBuffer Key((void*)"1234567890abcdef", 16, true);
	CBuffer CipherText;
	CBuffer Text2;

	CEncryption Encryption;
	Status = Encryption.SetPassword(CBuffer("1234567890", 10, true));

	Status = Encryption.Encrypt(Text1, CipherText);
	Status = Encryption.Decrypt(CipherText, Text2);

	CPrivateKey PrivateKey;
	CPublicKey PublicKey;
	PrivateKey.MakeKeyPair(PublicKey);

	CBuffer Hash((void*)"Hello, World 123", 16, true);
	CBuffer Signature;
	Status = PrivateKey.Sign(Hash, Signature);
	Status = PublicKey.Verify(Hash, Signature);*/

#endif
	
	m_pSidResolver = new CSidResolver(this);
	m_pSidResolver->Init();

	m_pProcessList = new CProcessList(this);
	m_pEnclaveManager = new CEnclaveManager(this);
	m_pProgramManager = new CProgramManager(this);
	m_pAccessManager = new CAccessManager(this);

	m_pNetworkManager = new CNetworkManager(this);

	m_pVolumeManager = new CVolumeManager(this);

	m_pTweakManager = new CTweakManager(this);

	connect(m_pProgramManager, SIGNAL(ProgramsAdded()), this, SIGNAL(ProgramsAdded()));

	connect(this, SIGNAL(ResRulesChanged()), m_pVolumeManager, SLOT(UpdateProtectedFolders()));

	//
	// Note trace log events always come from the service even though thay are generated by the driver 
	// Rule events are generated by the driver except for firewal
	// 
	// TODO: add own firewall engine to the driver
	//

	//m_Driver.RegisterRuleEventHandler(ERuleType::eAccess, &CPrivacyCore::OnDrvEvent, this);
	//m_Driver.RegisterRuleEventHandler(ERuleType::eProgram, &CPrivacyCore::OnDrvEvent, this);

	//m_Service.RegisterEventHandler(SVC_API_EVENT_PROG_ITEM_CHANGED, &CPrivacyCore::OnProgEvent, this);

	m_Service.RegisterEventHandler(SVC_API_EVENT_ENCLAVE_CHANGED, &CPrivacyCore::OnSvcEvent, this);
	m_Service.RegisterEventHandler(SVC_API_EVENT_FW_RULE_CHANGED, &CPrivacyCore::OnSvcEvent, this);
	m_Service.RegisterEventHandler(SVC_API_EVENT_EXEC_RULE_CHANGED, &CPrivacyCore::OnSvcEvent, this);
	m_Service.RegisterEventHandler(SVC_API_EVENT_RES_RULE_CHANGED, &CPrivacyCore::OnSvcEvent, this);

	m_Service.RegisterEventHandler(SVC_API_EVENT_NET_ACTIVITY, &CPrivacyCore::OnSvcEvent, this);
	m_Service.RegisterEventHandler(SVC_API_EVENT_EXEC_ACTIVITY, &CPrivacyCore::OnSvcEvent, this);
	m_Service.RegisterEventHandler(SVC_API_EVENT_RES_ACTIVITY, &CPrivacyCore::OnSvcEvent, this);

	m_Service.RegisterEventHandler(SVC_API_EVENT_CLEANUP_PROGRESS, &CPrivacyCore::OnCleanUpDone, this);

	CNtPathMgr::Instance()->RegisterDeviceChangeCallback(DeviceChangedCallback, this);
}

CPrivacyCore::~CPrivacyCore()
{
	CNtPathMgr::Instance()->UnRegisterDeviceChangeCallback(DeviceChangedCallback, this);
}

void CPrivacyCore::DeviceChangedCallback(void* param)
{
	CPrivacyCore* This = (CPrivacyCore*)param;
	emit This->DevicesChanged();
}

STATUS CPrivacyCore__RunAgent(const std::wstring& params)
{
	STATUS Status;

	std::wstring Path = GetApplicationDirectory() + L"\\" API_SERVICE_BINARY;

	HANDLE hEngineProcess = RunElevated(Path, params);
	if (!hEngineProcess)
		return ERR(STATUS_UNSUCCESSFUL);
	if (WaitForSingleObject(hEngineProcess, 30 * 1000) == WAIT_OBJECT_0) {
		DWORD exitCode;
		GetExitCodeProcess(hEngineProcess, &exitCode);
		if(exitCode != 0)
			Status = ERR(exitCode);
	} else
		Status = ERR(STATUS_TIMEOUT);
	CloseHandle(hEngineProcess);

	return Status;
}

STATUS CPrivacyCore::Install()
{
	return CPrivacyCore__RunAgent(L"-install");
}

STATUS CPrivacyCore::Uninstall()
{
	return CPrivacyCore__RunAgent(L"-remove");
}

bool CPrivacyCore::IsInstalled()
{
	SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
	return ((SvcState & SVC_INSTALLED) == SVC_INSTALLED);
}

bool CPrivacyCore::SvcIsRunning()
{
	SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
	return ((SvcState & SVC_RUNNING) == SVC_RUNNING);
}

STATUS CPrivacyCore::Connect(bool bEngineMode)
{
	STATUS Status;
	if (!m_Service.IsConnected())
	{
		m_bEngineMode = bEngineMode;
		if (bEngineMode)
			Status = m_Service.ConnectEngine();
		else
		{
			SVC_STATE SvcState = GetServiceState(API_SERVICE_NAME);
			if ((SvcState & SVC_RUNNING) == 0)
				Status = CPrivacyCore__RunAgent(L"-startup");

			if (Status)
			{
				for (int i = 0; i < 10; i++) 
				{
					Status = m_Service.ConnectSvc();
					if(Status)
						break;
					QThread::sleep(1+i);
				}
			}
		}

		if (Status) {
			uint32 ServiceABI = m_Service.GetABIVersion();
			if(!ServiceABI)
				return ERR(STATUS_PIPE_DISCONNECTED, L"Service NOT Available"); // STATUS_PORT_DISCONNECTED
			if(ServiceABI != MY_ABI_VERSION)
				return ERR(STATUS_REVISION_MISMATCH, L"Service ABI Mismatch");
		} else 
			return Status;
	}

	m_ConfigDir = QueryConfigDir();

	if (!m_Driver.IsConnected()) 
	{
		Status = m_Driver.ConnectDrv();

		if (Status) {
			uint32 DriverABI = m_Driver.GetABIVersion();
			if(!DriverABI)
				return ERR(STATUS_DEVICE_NOT_CONNECTED, L"Driver NOT Available");
			if(DriverABI != MY_ABI_VERSION)
				return ERR(STATUS_REVISION_MISMATCH, L"Driver ABI Mismatch");

			m_Driver.RegisterForConfigEvents(EConfigGroup::eEnclaves);
			m_Driver.RegisterForConfigEvents(EConfigGroup::eAccessRules);
			m_Driver.RegisterForConfigEvents(EConfigGroup::eProgramRules);
		}
	}
	return Status;
}

void CPrivacyCore::Disconnect(bool bKeepEngine)
{
	m_Driver.Disconnect();

	if (m_bEngineMode && !bKeepEngine)
		m_Service.Call(SVC_API_SHUTDOWN, CVariant());
	m_Service.Disconnect();

	m_EnclavesUpToDate = false;
	m_ProgramRulesUpToDate = false;
	m_AccessRulesUpToDate = false;
	m_FwRulesUpToDate = false;
}

STATUS CPrivacyCore::Update()
{
	STATUS Status;
	Status = m_pProcessList->Update(); if (!Status) return Status;
	Status = m_pEnclaveManager->Update(); if (!Status) return Status;
	Status = m_pProgramManager->Update(); if (!Status) return Status;
	Status = m_pAccessManager->Update(); if (!Status) return Status;

	return OK;
}

void CPrivacyCore::ProcessEvents()
{
	QMutexLocker Lock(&m_EventQueueMutex);
	//auto ProcRuleEvents = m_DrvEventQueue.take(ERuleType::eProgram);
	//auto ResRuleEvents = m_DrvEventQueue.take(ERuleType::eAccess);
	
	auto EnclaveEvents = m_SvcEventQueue.take(SVC_API_EVENT_ENCLAVE_CHANGED);
	auto FwRuleEvents = m_SvcEventQueue.take(SVC_API_EVENT_FW_RULE_CHANGED);
	auto ExecRuleEvents = m_SvcEventQueue.take(SVC_API_EVENT_EXEC_RULE_CHANGED);
	auto ResRuleEvents = m_SvcEventQueue.take(SVC_API_EVENT_RES_RULE_CHANGED);

	auto NetEvents = m_SvcEventQueue.take(SVC_API_EVENT_NET_ACTIVITY);
	auto ExecEvents = m_SvcEventQueue.take(SVC_API_EVENT_EXEC_ACTIVITY);
	auto ResEvents = m_SvcEventQueue.take(SVC_API_EVENT_RES_ACTIVITY);
	Lock.unlock();

	//////////////////
	// Enclaved

	if (!m_EnclavesUpToDate) {
		if (m_pEnclaveManager->UpdateAllEnclaves()) {
			m_EnclavesUpToDate = true;
			emit EnclavesChanged();
		}
	}
	else if(!EnclaveEvents.isEmpty()) {
		foreach(const XVariant& vEvent, EnclaveEvents) {
			QFlexGuid Guid;
			Guid.FromVariant(vEvent[API_V_GUID]);
			if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eRemoved)
				m_pEnclaveManager->RemoveEnclave(Guid);
			else
				m_pEnclaveManager->UpdateEnclave(Guid);
		}
		emit EnclavesChanged();
	}

	//////////////////
	// Rules

	if (!m_ProgramRulesUpToDate) {
		if (m_pProgramManager->UpdateAllProgramRules()) {
			m_ProgramRulesUpToDate = true;
			emit ExecRulesChanged();
		}
	}
	else if(!ExecRuleEvents.isEmpty()) {
		foreach(const XVariant& vEvent, ExecRuleEvents) {
			QFlexGuid Guid;
			Guid.FromVariant(vEvent[API_V_GUID]);
			if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eRemoved)
				m_pProgramManager->RemoveProgramRule(Guid);
			else
				m_pProgramManager->UpdateProgramRule(Guid);
		}
		emit ExecRulesChanged();
	}

	if (!m_AccessRulesUpToDate) {
		if (m_pAccessManager->UpdateAllAccessRules()) {
			m_AccessRulesUpToDate = true;
			emit ResRulesChanged();
		}
	}
	else if(!ResRuleEvents.isEmpty()){
		foreach(const XVariant& vEvent, ResRuleEvents) {
			QFlexGuid Guid;
			Guid.FromVariant(vEvent[API_V_GUID]);
			if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eRemoved)
				m_pAccessManager->RemoveAccessRule(Guid);
			else
				m_pAccessManager->UpdateAccessRule(Guid);
		}
		emit ResRulesChanged();
	}

	if (!m_FwRulesUpToDate) {
		if (m_pNetworkManager->UpdateAllFwRules()) {
			m_FwRulesUpToDate = true;
			emit FwRulesChanged();
		}
	}
	else if(!FwRuleEvents.isEmpty()){
		foreach(const XVariant& vEvent, FwRuleEvents) {
			QFlexGuid Guid;
			Guid.FromVariant(vEvent[API_V_GUID]);
			if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eRemoved)
				m_pNetworkManager->RemoveFwRule(Guid);
			else
				m_pNetworkManager->UpdateFwRule(Guid);
		}
		emit FwRulesChanged();
	}

	//////////////////
	// Events

	foreach(const XVariant& Event, NetEvents)
	{
		//QJsonDocument doc(QJsonValue::fromVariant(Event.ToQVariant()).toObject());			
		//QByteArray data = doc.toJson();

		CProgramID ID;
		ID.FromVariant(Event[API_V_PROG_ID]);
		CProgramFilePtr pProgram = m_pProgramManager->GetProgramFile(ID.GetFilePath());
		if (!pProgram) continue;

		CNetLogEntry* pNetEnrty = new CNetLogEntry();
		CLogEntryPtr pEntry = CLogEntryPtr(pNetEnrty);
		pEntry->FromVariant(Event[API_V_EVENT_DATA]);
		pProgram->TraceLogAdd(ETraceLogs::eNetLog, pEntry, Event[API_V_EVENT_INDEX]);

		if (pNetEnrty->GetState() == EFwEventStates::UnRuled)
			emit UnruledFwEvent(pProgram, pEntry);

		foreach(const CFwRulePtr& pRule, m_pNetworkManager->GetFwRules(pNetEnrty->GetAllowRules() | pNetEnrty->GetBlockRules()))
			pRule->IncrHitCount();
	}

	foreach(const XVariant& Event, ExecEvents)
	{
		CProgramID ID;
		ID.FromVariant(Event[API_V_PROG_ID]);
		CProgramFilePtr pProgram = m_pProgramManager->GetProgramFile(ID.GetFilePath());
		if (!pProgram) continue;

		CExecLogEntry* pExecEnrty = new CExecLogEntry();
		CLogEntryPtr pEntry = CLogEntryPtr(pExecEnrty);
		pEntry->FromVariant(Event[API_V_EVENT_DATA]);
		quint64 LogIndex = Event[API_V_EVENT_INDEX];
		if(LogIndex != -1)
			pProgram->TraceLogAdd(ETraceLogs::eExecLog, pEntry, LogIndex);

		switch (pExecEnrty->GetType())
		{
		case EExecLogType::eImageLoad:
			if (pExecEnrty->GetStatus() == EEventStatus::eBlocked)
				emit ExecutionEvent(pProgram, pEntry);
			break;
		case EExecLogType::eProcessStarted:
			if (pExecEnrty->GetRole() == EExecLogRole::eActor && pExecEnrty->GetStatus() == EEventStatus::eProtected)
				emit ExecutionEvent(pProgram, pEntry);
			break;
		}
	}

	foreach(const XVariant& Event, ResEvents)
	{
		CProgramID ID;
		ID.FromVariant(Event[API_V_PROG_ID]);
		CProgramFilePtr pProgram = m_pProgramManager->GetProgramFile(ID.GetFilePath());
		if (!pProgram) continue;

		CResLogEntry* pResEnrty = new CResLogEntry();
		CLogEntryPtr pEntry = CLogEntryPtr(pResEnrty);
		pEntry->FromVariant(Event[API_V_EVENT_DATA]);
		pProgram->TraceLogAdd(ETraceLogs::eResLog, pEntry, Event[API_V_EVENT_INDEX]);

		if(pResEnrty->GetStatus() == EEventStatus::eProtected)
			emit AccessEvent(pProgram, pEntry);
	}
}

//void CPrivacyCore::OnProgEvent(uint32 MessageId, const CBuffer* pEvent)
//{
//	// WARNING: this function is invoked from a worker thread !!!
//
//}

void CPrivacyCore::OnSvcEvent(uint32 MessageId, const CBuffer* pEvent)
{
	// WARNING: this function is invoked from a worker thread !!!

	XVariant vEvent;
	try {
		vEvent.FromPacket(pEvent);
	}
	catch (const CException&)
	{
		ASSERT(0);
		return;
	}

	switch(MessageId)
	{
	case SVC_API_EVENT_ENCLAVE_CHANGED:
		if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eAllChanged)
			m_EnclavesUpToDate = false;
		break;
	case SVC_API_EVENT_RES_RULE_CHANGED:
		if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eAllChanged)
			m_ProgramRulesUpToDate = false;
		break;
	case SVC_API_EVENT_EXEC_RULE_CHANGED:
		if (vEvent[API_V_EVENT_TYPE].To<uint32>() == (uint32)EConfigEvent::eAllChanged)
			m_AccessRulesUpToDate = false;
		break;
	}

	QMutexLocker Lock(&m_EventQueueMutex);
	m_SvcEventQueue[MessageId].enqueue(vEvent);
}

void CPrivacyCore::OnCleanUpDone(uint32 MessageId, const CBuffer* pEvent)
{
	// WARNING: this function is invoked from a worker thread !!!

	XVariant vEvent;
	try {
		vEvent.FromPacket(pEvent);
	}
	catch (const CException&)
	{
		ASSERT(0);
		return;
	}

	if (vEvent[API_V_PROGRESS_FINISHED].To<bool>())
		emit CleanUpDone();
	else
		emit CleanUpProgress(vEvent[API_V_PROGRESS_DONE].To<quint64>(), vEvent[API_V_PROGRESS_TOTAL].To<quint64>());
}

void CPrivacyCore::Clear()
{
	m_pProgramManager->Clear();
}

void CPrivacyCore::OnClearTraceLog(const CProgramItemPtr& pItem, ETraceLogs Log)
{
	if (auto pProgram = pItem.objectCast<CProgramFile>())
		pProgram->ClearLogs(Log);
	if (auto pService = pItem.objectCast<CWindowsService>())
		pService->ClearLogs(Log);
}

//void CPrivacyCore::OnDrvEvent(const std::wstring& Guid, EConfigEvent Event, ERuleType Type)
//{
//	// WARNING: this function is invoked from a worker thread !!!
//
//	QMutexLocker Lock(&m_EventQueueMutex);
//	m_DrvEventQueue[Type].enqueue(SDrvRuleEvent { QString::fromStdWString(Guid), Event });
//}

QString CPrivacyCore::NormalizePath(QString sPath, bool bForID)
{
	if(sPath.isEmpty() || sPath[0] == '*')
		return sPath;

	std::wstring Path = sPath.toStdWString();

	if (Path.length() >= 7 && Path.compare(0, 4, L"\\??\\") == 0 && Path.compare(5, 2, L":\\") == 0) // \??\X:\...
		Path.erase(0, 4);
	else if (Path.length() >= 7 && Path.compare(0, 4, L"\\\\?\\") == 0 && Path.compare(5, 2, L":\\") == 0) // \\?\X:\...
		Path.erase(0, 4);

	if (Path.find(L'%') != std::wstring::npos)
		Path = ExpandEnvironmentVariablesInPath(Path);

	if (MatchPathPrefix(Path, L"\\SystemRoot")) {
		static WCHAR windir[MAX_PATH + 8] = { 0 };
		if (!*windir) GetWindowsDirectoryW(windir, MAX_PATH);
		Path = windir + Path.substr(11);
	}

	if (!bForID && _wcsnicmp(Path.c_str(), L"\\device\\mup\\", 12) == 0)
		Path = L"\\\\" + Path.substr(12);
	else

	if (!CNtPathMgr::IsDosPath(Path) && _wcsnicmp(Path.c_str(), L"\\REGISTRY\\", 10) != 0)
	{
		if (_wcsnicmp(Path.c_str(), L"\\Device\\", 8) == 0)
			Path = L"\\\\.\\" + Path.substr(8);
		
		if (Path.length() >= 4 && Path.compare(0, 4, L"\\\\.\\") == 0) 
		{
			std::wstring DevicePath = L"\\Device\\" + Path.substr(4);
			std::wstring DosPath = CNtPathMgr::Instance()->TranslateNtToDosPath(DevicePath);
			if (!DosPath.empty())
				Path = DosPath;
			else if (bForID) 
				Path = DevicePath;
		}
	}

	sPath = QString::fromStdWString(Path);
	return bForID ? sPath.toLower() : sPath;
}

STATUS CPrivacyCore::HashFile(const QString& Path, CBuffer& Hash)
{
	QFile File(Path);
	if (!File.open(QIODevice::ReadOnly))
		return ERR(STATUS_NOT_FOUND);

	CHashFunction HashFunction;
	STATUS Status = HashFunction.InitHash();
	if (Status.IsError()) return Status;

	CBuffer Buffer(0x1000);
	for (;;) {
		int Read = File.read((char*)Buffer.GetBuffer(), Buffer.GetCapacity());
		if (Read <= 0) break;
		Buffer.SetSize(Read);
		Status = HashFunction.UpdateHash(Buffer);
		if (Status.IsError()) return Status;
	}

	Status = HashFunction.FinalizeHash(Hash);
	if (Status.IsError()) return Status;

	return OK;
}

QByteArray CPrivacyCore__MakeFileSig(const CBuffer& Signature, const QString& Path)
{
	XVariant SigData;
	// Note: the driver supportrs also teh V version
	SigData[API_S_VERSION] = DEF_MP_SIG_VERSION;
	SigData[API_S_SIGNATURE] = CVariant(Signature);
	SigData[API_S_TYPE] = "File";
	SigData[API_S_FILE_PATH] = Path;

	CBuffer SigBuffer;
	SigData.ToPacket(&SigBuffer);
	return QByteArray((char*)SigBuffer.GetBuffer(), SigBuffer.GetSize());
}

QByteArray CPrivacyCore__MakeCertSig(const CBuffer& Signature, const QString& Subject)
{
	XVariant SigData;
	// Note: the driver supportrs also teh V version
	SigData[API_S_VERSION] = DEF_MP_SIG_VERSION;
	SigData[API_S_SIGNATURE] = CVariant(Signature);
	SigData[API_S_TYPE] = "Certificate";
	SigData[API_S_NAME] = Subject;

	CBuffer SigBuffer;
	SigData.ToPacket(&SigBuffer);
	return QByteArray((char*)SigBuffer.GetBuffer(), SigBuffer.GetSize());
}

STATUS CPrivacyCore::SignFiles(const QStringList& Paths, const CPrivateKey* pPrivateKey)
{
	STATUS Status;
	foreach(const QString & Path, Paths)
	{
		CBuffer Hash;
		Status = HashFile(Path, Hash);

		CBuffer Signature;
		Status = pPrivateKey->Sign(Hash, Signature);
		if (Status.IsError()) continue;

		QString Name = Split2(Path, "\\", true).second;
		QString SignaturePath = "\\sig_db\\" + Name;
		SignaturePath += "\\" + QByteArray((char*)Hash.GetBuffer(), Hash.GetSize()).toHex().toUpper() + ".mpsig";

		Status = WriteConfigFile(SignaturePath, CPrivacyCore__MakeFileSig(Signature, Path));
		if(!Status.IsError())
			m_SigFileCache[Path.toLower()] = 1;
	}
	return Status;
}

QString CPrivacyCore::GetSignatureFilePath(const QString& Path)
{
	QString Name = Split2(Path, "\\", true).second;
	QString SignaturePath = Name + ".mpsig";
	if (QFile::exists(SignaturePath))
		return SignaturePath;
	
	SignaturePath = "\\sig_db\\" + Name;
	if (!CheckConfigFile(SignaturePath)) return "";
		
	CBuffer Hash;
	STATUS Status = HashFile(Path, Hash);
	if (Status.IsError()) return "";
	
	SignaturePath += "\\" + QByteArray((char*)Hash.GetBuffer(), Hash.GetSize()).toHex().toUpper() + ".mpsig";
	return SignaturePath;
}

bool CPrivacyCore::HasFileSignature(const QString& Path)
{
	int State = m_SigFileCache.value(Path.toLower(), -1);
	if(State != -1)
		return State == 1;
	State = 0;

	QString SignaturePath = GetSignatureFilePath(Path);
	if (!SignaturePath.isEmpty()) {
		if (SignaturePath.at(0) == '\\' ? CheckConfigFile(SignaturePath) : QFile::exists(SignaturePath))
			State = 1;
	}

	m_SigFileCache.insert(Path.toLower(), State);
	return State == 1;
}

STATUS CPrivacyCore::RemoveFileSignature(const QStringList& Paths)
{
	STATUS Status;
	foreach(const QString& Path, Paths)
	{
		QString SignaturePath = GetSignatureFilePath(Path);
		if (!SignaturePath.isEmpty()) {
			if (SignaturePath.at(0) == '\\')
				Status = RemoveConfigFile(SignaturePath);
			else
				Status = QFile::remove(SignaturePath) ? OK : ERR(STATUS_UNSUCCESSFUL);
		}
		if(!Status.IsError())
			m_SigFileCache[Path.toLower()] = 0;
	}
	return Status;
}

STATUS CPrivacyCore::SignCerts(const QMap<QByteArray, QString>& Certs, const class CPrivateKey* pPrivateKey)
{
	STATUS Status;
	for(auto I = Certs.begin(); I != Certs.end(); ++I)
	{
		QByteArray SignerHash = I.key();
		QString Subject = I.value();
		
		CBuffer Signature;
		Status = pPrivateKey->Sign(CBuffer((void*)SignerHash.data(), SignerHash.size(), true), Signature);
		if (Status.IsError()) continue;

		QString SignaturePath = "\\sig_db\\" + Subject;
		QDir().mkpath(SignaturePath);
		SignaturePath += "\\" + SignerHash.toHex().toUpper() + ".mpsig";
		
		Status = WriteConfigFile(SignaturePath, CPrivacyCore__MakeCertSig(Signature, Subject));
		if(!Status.IsError())
			m_SigFileCache[Subject.toLower() + "/" + SignerHash] = 1;
	}
	return Status;
}

bool CPrivacyCore::HasCertSignature(const QString& Subject, const QByteArray& SignerHash)
{
	if(SignerHash.isEmpty())
		return false;

	int State = m_SigFileCache.value(Subject.toLower() + "/" + SignerHash, -1);
	if(State != -1)
		return State == 1;
	State = 0;

	QString SignaturePath = "\\sig_db\\" + Subject;
	SignaturePath += "\\" + SignerHash.toHex().toUpper() + ".mpsig";

	if (CheckConfigFile(SignaturePath))
		State = 1;

	m_SigFileCache.insert(Subject.toLower() + "/" + SignerHash, State);
	return State == 1;
}

STATUS CPrivacyCore::RemoveCertSignature(const QMap<QByteArray, QString>& Certs)
{
	STATUS Status;
	for(auto I = Certs.begin(); I != Certs.end(); ++I)
	{
		QByteArray SignerHash = I.key();
		QString Subject = I.value();

		QString SignaturePath = "\\sig_db\\" + Subject;
		SignaturePath += "\\" + SignerHash.toHex().toUpper() + ".mpsig";

		Status = RemoveConfigFile(SignaturePath);
		if(!Status.IsError())
			m_SigFileCache[Subject.toLower() + "/" + SignerHash] = 0;
	}
	return Status;
}

XVariant CPrivacyCore::MakeIDs(const QList<const class CProgramItem*>& Nodes)
{
	SVarWriteOpt Opts;

	XVariant IDs;
	foreach(auto Item, Nodes) 
	{
		IDs.Append(Item->GetID().ToVariant(Opts));
	}
	return IDs;
}

#define RET_AS_XVARIANT(r) \
auto Ret = r; \
if (Ret.IsError()) \
return ERR(Ret.GetStatus()); \
RETURN((XVariant&)Ret.GetValue());

#define RET_GET_XVARIANT(r, n) \
auto Ret = r; \
if (Ret.IsError()) \
return ERR(Ret.GetStatus()); \
CVariant& Res = Ret.GetValue(); \
RETURN((XVariant&)Res.Get(n));

QString CPrivacyCore::QueryConfigDir()
{
	CVariant Request;
	auto Ret = m_Service.Call(SVC_API_GET_CONFIG_DIR, Request);
	if (Ret.IsError())
		return "";
	XVariant& Response = (XVariant&)Ret.GetValue();
	return Response[API_V_VALUE].AsQStr();
}

bool CPrivacyCore::CheckConfigFile(const QString& Name)
{
	CVariant Request;
	Request[API_V_FILE_PATH] = Name.toStdString();
	auto Ret = m_Service.Call(SVC_API_CHECK_CONFIG_FILE, Request);
	if (Ret.IsError())
		return false;
	XVariant& Response = (XVariant&)Ret.GetValue();
	return Response.To<bool>();
}

RESULT(QByteArray) CPrivacyCore::ReadConfigFile(const QString& Name)
{
	CVariant Request;
	Request[API_V_FILE_PATH] = Name.toStdString();
	auto Ret = m_Service.Call(SVC_API_GET_CONFIG_FILE, Request);
	if (Ret.IsError())
		return Ret.GetStatus();
	XVariant& Response = (XVariant&)Ret.GetValue();
	return Response[API_V_DATA].AsQBytes();
}

STATUS CPrivacyCore::WriteConfigFile(const QString& Name, const QByteArray& Data)
{
	CVariant Request;
	Request[API_V_FILE_PATH] = Name.toStdString();
	Request[API_V_DATA] = XVariant(Data);
	return m_Service.Call(SVC_API_SET_CONFIG_FILE, Request);
}

STATUS CPrivacyCore::RemoveConfigFile(const QString& Name)
{
	CVariant Request;
	Request[API_V_FILE_PATH] = Name.toStdString();
	return m_Service.Call(SVC_API_DEL_CONFIG_FILE, Request);
}

RESULT(QStringList) CPrivacyCore::ListConfigFiles(const QString& Name)
{
	CVariant Request;
	Request[API_V_FILE_PATH] = Name.toStdString();
	auto Ret = m_Service.Call(SVC_API_LIST_CONFIG_FILES, Request);
	if (Ret.IsError())
		return Ret.GetStatus();
	//
	// Note: the returned names are sufixed with L'\\' if they represent a directory
	//
	XVariant& Response = (XVariant&)Ret.GetValue();
	return Response[API_V_FILES].AsQList();
}

RESULT(XVariant) CPrivacyCore::GetConfig(const QString& Name) 
{
	CVariant Request;
	Request[API_V_KEY] = Name.toStdString();
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_CONFIG, Request), API_V_VALUE);
}

STATUS CPrivacyCore::SetConfig(const QString& Name, const XVariant& Value)
{
	CVariant Request;
	Request[API_V_KEY] = Name.toStdString();
	Request[API_V_VALUE] = Value;
	return m_Service.Call(SVC_API_SET_CONFIG, Request);
}

RESULT(XVariant) CPrivacyCore::GetSvcConfig()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_CONFIG, Request), API_V_DATA);
}

STATUS CPrivacyCore::SetSvcConfig(const XVariant& Data)
{
	CVariant Request;
	Request[API_V_DATA] = Data;
	return m_Service.Call(SVC_API_SET_CONFIG, Request);
}

RESULT(XVariant) CPrivacyCore::GetDrvConfig()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Driver.Call(API_GET_CONFIG_VALUE, Request), API_V_DATA);
}

STATUS CPrivacyCore::SetDrvConfig(const XVariant& Data)
{
	CVariant Request;
	Request[API_V_DATA] = Data;
	return m_Driver.Call(API_SET_CONFIG_VALUE, Request);
}

// Process Manager
RESULT(XVariant) CPrivacyCore::GetProcesses()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_PROCESSES, Request), API_V_PROCESSES);
}

RESULT(XVariant) CPrivacyCore::GetProcess(uint64 Pid)
{
	CVariant Request;
	Request[API_V_PID] = Pid;
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_PROCESS, Request))
}

STATUS CPrivacyCore::TerminateProcess(uint64 Pid)
{
	//CVariant Request;
	//Request[API_V_PID] = Pid;
	//return m_Service.Call(SVC_API_TERMINATE_PROCESS, Request);

	NTSTATUS status;

	OBJECT_ATTRIBUTES objectAttributes;
	InitializeObjectAttributes(&objectAttributes, NULL, 0, NULL, NULL);

	CLIENT_ID clientId;
	clientId.UniqueProcess = (HANDLE)Pid;
	clientId.UniqueThread = NULL;
	
	CScopedHandle hProcess((HANDLE)NULL, CloseHandle);
	status = NtOpenProcess(&hProcess, PROCESS_TERMINATE, &objectAttributes, &clientId);
	if (NT_SUCCESS(status))
		status = NtTerminateProcess(hProcess, STATUS_SUCCESS);
	if(!NT_SUCCESS(status))
		return ERR(status);
	return OK;
}

// Secure Enclaves
STATUS CPrivacyCore::SetAllEnclaves(const XVariant& Enclaves)
{
	return m_Driver.Call(API_SET_ENCLAVES, Enclaves);
}

RESULT(XVariant) CPrivacyCore::GetAllEnclaves()
{
	CVariant Request;
	RET_AS_XVARIANT(m_Driver.Call(API_GET_ENCLAVES, Request));
}

STATUS CPrivacyCore::SetEnclave(const XVariant& Enclave)
{
	return m_Driver.Call(API_SET_ENCLAVE, Enclave);
}

RESULT(XVariant) CPrivacyCore::GetEnclave(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	RET_AS_XVARIANT(m_Driver.Call(API_GET_ENCLAVE, Request));
}

STATUS CPrivacyCore::DelEnclave(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	return m_Driver.Call(API_DEL_ENCLAVE, Request);
}

STATUS CPrivacyCore::StartProcessInEnclave(const QString& Command, const QFlexGuid& Guid)
{
	STATUS Status = theCore->Driver()->PrepareEnclave(Guid);
	if (Status.IsError())
		return Status;

	STARTUPINFOW si = { 0 };
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = { 0 };
	if (CreateProcessW(NULL, (wchar_t*)Command.utf16(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pi.dwProcessId, true);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if(!pProcess)
			return ERR(STATUS_UNSUCCESSFUL);

		KPH_PROCESS_SFLAGS SecFlags;
		SecFlags.SecFlags = pProcess->GetSecFlags();
		if (SecFlags.EjectFromEnclave)
			return ERR(STATUS_ERR_PROC_EJECTED);
	} 
	else
		return ERR(STATUS_UNSUCCESSFUL); // todo make a better error code
	return OK;
}

// Program Manager
RESULT(XVariant) CPrivacyCore::GetPrograms()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_PROGRAMS, Request), API_V_PROGRAMS);
}

RESULT(XVariant) CPrivacyCore::GetLibraries(uint64 CacheToken)
{
	CVariant Request;
	if(CacheToken != -1)
		Request[API_V_CACHE_TOKEN] = CacheToken;
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_LIBRARIES, Request));
}

RESULT(quint64) CPrivacyCore::SetProgram(const CProgramItemPtr& pItem)
{
	SVarWriteOpt Opts;

	auto Ret = m_Service.Call(SVC_API_SET_PROGRAM, pItem->ToVariant(Opts));
	if (Ret.IsError())
		return Ret;
	CVariant& Response = Ret.GetValue();
	RETURN(Response.Get(API_V_PROG_UID).To<uint64>());
}

STATUS CPrivacyCore::AddProgramTo(uint64 UID, uint64 ParentUID)
{
	CVariant Request;
	Request[API_V_PROG_UID] = UID;
	Request[API_V_PROG_PARENT] = ParentUID;
	return m_Service.Call(SVC_API_ADD_PROGRAM, Request);
}

STATUS CPrivacyCore::RemoveProgramFrom(uint64 UID, uint64 ParentUID, bool bDelRules)
{
	CVariant Request;
	Request[API_V_PROG_UID] = UID;
	Request[API_V_PROG_PARENT] = ParentUID;
	Request[API_V_DEL_WITH_RULES] = bDelRules;
	return m_Service.Call(SVC_API_REMOVE_PROGRAM, Request);
}

STATUS CPrivacyCore::CleanUpPrograms(bool bPurgeRules)
{
	CVariant Request;
	Request[API_V_PURGE_RULES] = bPurgeRules;
	return m_Service.Call(SVC_API_CLEANUP_PROGRAMS, Request);
}

STATUS CPrivacyCore::ReGroupPrograms()
{
	CVariant Request;
	return m_Service.Call(SVC_API_REGROUP_PROGRAMS, Request);
}

STATUS CPrivacyCore::SetAllProgramRules(const XVariant& Rules)
{
	return m_Driver.Call(API_SET_PROGRAM_RULES, Rules);
}

RESULT(XVariant) CPrivacyCore::GetAllProgramRules()
{
	CVariant Request;
	RET_AS_XVARIANT(m_Driver.Call(API_GET_PROGRAM_RULES, Request));
}

STATUS CPrivacyCore::SetProgramRule(const XVariant& Rule)
{
	return m_Driver.Call(API_SET_PROGRAM_RULE, Rule);
}

RESULT(XVariant) CPrivacyCore::GetProgramRule(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	RET_AS_XVARIANT(m_Driver.Call(API_GET_PROGRAM_RULE, Request));
}

STATUS CPrivacyCore::DelProgramRule(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	return m_Driver.Call(API_DEL_PROGRAM_RULE, Request);
}

// Access Manager
STATUS CPrivacyCore::SetAllAccessRules(const XVariant& Rules)
{
	return m_Driver.Call(API_SET_ACCESS_RULES, Rules);
}

RESULT(XVariant) CPrivacyCore::GetAllAccessRules()
{
	CVariant Request;
	RET_AS_XVARIANT(m_Driver.Call(API_GET_ACCESS_RULES, Request));
}

STATUS CPrivacyCore::SetAccessRule(const XVariant& Rule)
{
	return m_Driver.Call(API_SET_ACCESS_RULE, Rule);
}

RESULT(XVariant) CPrivacyCore::GetAccessRule(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	RET_AS_XVARIANT(m_Driver.Call(API_GET_ACCESS_RULE, Request));
}

STATUS CPrivacyCore::DelAccessRule(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	return m_Driver.Call(API_DEL_ACCESS_RULE, Request);
}

// Network Manager
RESULT(XVariant) CPrivacyCore::GetFwRulesFor(const QList<const class CProgramItem*>& Nodes)
{
	CVariant Request;
	if(!Nodes.isEmpty())
		Request[API_V_PROG_IDS] = MakeIDs(Nodes);
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_FW_RULES, Request), API_V_FW_RULES);
}

RESULT(XVariant) CPrivacyCore::GetAllFwRules(bool bReLoad)
{
	CVariant Request;
	if(bReLoad)
		Request[API_V_RELOAD] = true;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_FW_RULES, Request), API_V_FW_RULES);
}

STATUS CPrivacyCore::SetFwRule(const XVariant& FwRule)
{
	return m_Service.Call(SVC_API_SET_FW_RULE, FwRule);
}

RESULT(XVariant) CPrivacyCore::GetFwRule(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_FW_RULE, Request));
}

STATUS CPrivacyCore::DelFwRule(const QFlexGuid& Guid)
{
	CVariant Request;
	Request[API_V_GUID] = Guid.ToVariant(true);
	return m_Service.Call(SVC_API_DEL_FW_RULE, Request);
}

RESULT(FwFilteringModes) CPrivacyCore::GetFwProfile()
{
	CVariant Request;
	auto Ret = m_Service.Call(SVC_API_GET_FW_PROFILE, Request);
	if (Ret.IsError())
		return Ret;
	CVariant& Response = Ret.GetValue();
	return (FwFilteringModes) Response.Get(API_V_FW_RULE_FILTER_MODE).To<uint32>();
}

STATUS CPrivacyCore::SetFwProfile(FwFilteringModes Profile)
{
	CVariant Request;
	Request[API_V_FW_RULE_FILTER_MODE] = (uint32)Profile;
	return m_Service.Call(SVC_API_SET_FW_PROFILE, Request);
}

RESULT(FwAuditPolicy) CPrivacyCore::GetAuditPolicy()
{
	CVariant Request;
	auto Ret = m_Service.Call(SVC_API_GET_FW_AUDIT_MODE, Request);
	if (Ret.IsError())
		return Ret;
	CVariant& Response = Ret.GetValue();
	return (FwAuditPolicy) Response.Get(API_V_FW_AUDIT_MODE).To<uint32>();
}

STATUS CPrivacyCore::SetAuditPolicy(FwAuditPolicy Profile)
{
	CVariant Request;
	Request[API_V_FW_AUDIT_MODE] = (uint32)Profile;
	return m_Service.Call(SVC_API_SET_FW_AUDIT_MODE, Request);
}

RESULT(XVariant) CPrivacyCore::GetSocketsFor(const QList<const class CProgramItem*>& Nodes)
{
	CVariant Request;
	if(!Nodes.isEmpty())
		Request[API_V_PROG_IDS] = MakeIDs(Nodes);
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_SOCKETS, Request), API_V_SOCKETS);
}

RESULT(XVariant) CPrivacyCore::GetAllSockets()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_SOCKETS, Request), API_V_SOCKETS);
}

RESULT(XVariant) CPrivacyCore::GetTrafficLog(const class CProgramID& ID, quint64 MinLastActivity)
{
	CVariant Request;
	Request[API_V_PROG_ID] = ID.ToVariant(SVarWriteOpt());
	if(MinLastActivity) 
		Request[API_V_SOCK_LAST_NET_ACT] = MinLastActivity;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_TRAFFIC, Request), API_V_TRAFFIC_LOG);
}

RESULT(XVariant) CPrivacyCore::GetDnsCache()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_DNC_CACHE, Request), API_V_DNS_CACHE);
}

STATUS CPrivacyCore::FlushDnsCache()
{
	CVariant Request;
	return m_Service.Call(SVC_API_FLUSH_DNS_CACHE, Request);
}

// Access Manager

RESULT(XVariant) CPrivacyCore::GetHandlesFor(const QList<const class CProgramItem*>& Nodes)
{
	CVariant Request;
	if(!Nodes.isEmpty())
		Request[API_V_PROG_IDS] = MakeIDs(Nodes);
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_HANDLES, Request), API_V_HANDLES);
}

RESULT(XVariant) CPrivacyCore::GetAllHandles()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_HANDLES, Request), API_V_HANDLES);
}

STATUS CPrivacyCore::ClearTraceLog(ETraceLogs Log, const CProgramItemPtr& pItem)
{
	if(pItem)
		OnClearTraceLog(pItem, Log);
	else {
		foreach(auto pItem, m_pProgramManager->GetItems())
			OnClearTraceLog(pItem, Log);
	}

	CVariant Request;
	if(pItem) Request[API_V_PROG_ID] = pItem->GetID().ToVariant(SVarWriteOpt());
	Request[API_V_LOG_TYPE] = (int)Log;
	return m_Service.Call(SVC_API_CLEAR_LOGS, Request);
}

STATUS CPrivacyCore::CleanUpAccessTree()
{
	CVariant Request;
	return m_Service.Call(SVC_API_CLEANUP_ACCESS_TREE, Request);
}

// Program Item
RESULT(XVariant) CPrivacyCore::GetLibraryStats(const class CProgramID& ID)
{
	CVariant Request;
	Request[API_V_PROG_ID] = ID.ToVariant(SVarWriteOpt());
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_LIBRARY_STATS, Request), API_V_LIBRARIES);
}

STATUS CPrivacyCore::CleanUpLibraries(const CProgramItemPtr& pItem)
{
	if (pItem) {
		if (auto pProgram = pItem.objectCast<CProgramFile>())
			pProgram->SetLibrariesChanged();
	}
	else {
		foreach(auto pItem, m_pProgramManager->GetItems()) {
			if (auto pProgram = pItem.objectCast<CProgramFile>())
				pProgram->SetLibrariesChanged();
		}
	}

	CVariant Request;
	if(pItem) Request[API_V_PROG_ID] = pItem->GetID().ToVariant(SVarWriteOpt());
	return m_Service.Call(SVC_API_CLEANUP_LIBS, Request);
}

RESULT(XVariant) CPrivacyCore::GetExecStats(const class CProgramID& ID)
{
	CVariant Request;
	Request[API_V_PROG_ID] = ID.ToVariant(SVarWriteOpt());
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_EXEC_STATS, Request));
}

RESULT(XVariant) CPrivacyCore::GetIngressStats(const class CProgramID& ID)
{
	CVariant Request;
	Request[API_V_PROG_ID] = ID.ToVariant(SVarWriteOpt());
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_INGRESS_STATS, Request));
}

RESULT(XVariant) CPrivacyCore::GetAccessStats(const class CProgramID& ID, quint64 MinLastActivity)
{
	CVariant Request;
	Request[API_V_PROG_ID] = ID.ToVariant(SVarWriteOpt());
	if(MinLastActivity) 
		Request[API_V_LAST_ACTIVITY] = MinLastActivity;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_ACCESS_STATS, Request), API_V_PROG_RESOURCE_ACCESS);
}

RESULT(XVariant) CPrivacyCore::GetTraceLog(const class CProgramID& ID, ETraceLogs Log)
{
	CVariant Request;
	Request[API_V_PROG_ID] = ID.ToVariant(SVarWriteOpt());
	Request[API_V_LOG_TYPE] = (uint32)Log;
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_TRACE_LOG, Request));	
}

// Volume Manager
RESULT(XVariant) CPrivacyCore::GetVolumes()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_VOL_GET_ALL_VOLUMES, Request), API_V_VOLUMES);
}

STATUS CPrivacyCore::MountVolume(const QString& Path, const QString& MountPoint, const QString& Password, bool bProtect)
{
	CVariant Request;
	Request[API_V_VOL_PATH] = QString(Path).replace("/","\\").toStdWString();
	Request[API_V_VOL_MOUNT_POINT] = MountPoint.toStdWString();
	Request[API_V_VOL_PASSWORD] = Password.toStdWString();
	Request[API_V_VOL_PROTECT] = bProtect;
	return m_Service.Call(SVC_API_VOL_MOUNT_IMAGE, Request);
}

STATUS CPrivacyCore::DismountVolume(const QString& MountPoint)
{
	CVariant Request;
	Request[API_V_VOL_MOUNT_POINT] = MountPoint.toStdWString();
	return m_Service.Call(SVC_API_VOL_DISMOUNT_VOLUME, Request);
}

STATUS CPrivacyCore::DismountAllVolumes()
{
	CVariant Request;
	return m_Service.Call(SVC_API_VOL_DISMOUNT_ALL, Request);
}

STATUS CPrivacyCore::CreateVolume(const QString& Path, const QString& Password, quint64 ImageSize, const QString& Cipher)
{
	CVariant Request;
	Request[API_V_VOL_PATH] = QString(Path).replace("/","\\").toStdWString();
	Request[API_V_VOL_PASSWORD] = Password.toStdWString();
	if(ImageSize) Request[API_V_VOL_SIZE] = ImageSize;
	if(!Cipher.isEmpty()) Request[API_V_VOL_CIPHER] = Cipher.toStdWString();
	return m_Service.Call(SVC_API_VOL_CREATE_IMAGE, Request);
}

STATUS CPrivacyCore::ChangeVolumePassword(const QString& Path, const QString& OldPassword, const QString& NewPassword)
{
	CVariant Request;
	Request[API_V_VOL_PATH] = QString(Path).replace("/","\\").toStdWString();
	Request[API_V_VOL_OLD_PASS] = OldPassword.toStdWString();
	Request[API_V_VOL_NEW_PASS] = NewPassword.toStdWString();
	return m_Service.Call(SVC_API_VOL_CHANGE_PASSWORD, Request);
}

// Tweak Manager
RESULT(XVariant) CPrivacyCore::GetTweaks()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_TWEAKS, Request), API_V_TWEAKS);
}

STATUS CPrivacyCore::ApplyTweak(const QString& Name)
{
	CVariant Request;
	Request[API_V_NAME] = Name.toStdWString();
	return m_Service.Call(SVC_API_APPLY_TWEAK, Request);
}

STATUS CPrivacyCore::UndoTweak(const QString& Name)
{
	CVariant Request;
	Request[API_V_NAME] = Name.toStdWString();
	return m_Service.Call(SVC_API_UNDO_TWEAK, Request);
}

//
STATUS CPrivacyCore::SetWatchedPrograms(const QSet<CProgramItemPtr>& Programs)
{
	XVariant ProgramList;
	ProgramList.BeginList();
	foreach(auto pProgram, Programs)
		ProgramList.Write(pProgram->GetUID());
	ProgramList.Finish();

	XVariant Request;
	Request[API_V_PROG_UIDS] = ProgramList;
	return m_Service.Call(SVC_API_SET_WATCHED_PROG, Request);
}

// Support
RESULT(XVariant) CPrivacyCore::GetSupportInfo()
{
	CVariant Request;
	RET_AS_XVARIANT(m_Driver.Call(API_GET_SUPPORT_INFO, Request));
}