//#define _HAS_EXCEPTIONS 0

#include <windows.h>
#include <winevt.h>
#include <winmeta.h>
#include <NTSecAPI.h>
#include <string>
//#include <functional>
//#include <map>
#include <vector>
//#include <mutex>
//#include <algorithm>
//#include <atomic>
//#define ASSERT(x)
#include "EvtUtil.h"

//#pragma comment(lib, "wevtapi.lib")


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities to configure Auditing Policies

// defined in NTSecAPI
/*
    {0CCE9226-69AE-11D9-BED3-505054503030}
    Identifies the Filtering Platform Connection audit subcategory.
    This subcategory audits connections that are allowed or blocked by WFP.

    {0CCE9225-69AE-11D9-BED3-505054503030}
    Identifies the Filtering Platform Packet Drop audit subcategory.
    This subcategory audits packets that are dropped by Windows Filtering Platform (WFP).

    {0CCE9233-69AE-11D9-BED3-505054503030}
    Identifies the Filtering Platform Policy Change audit subcategory.
    This subcategory audits events generated by changes to Windows Filtering Platform (WFP).         
*/

GUID Audit_ObjectAccess_FirewallPacketDrops_ = { 0x0cce9225, 0x69ae, 0x11d9, { 0xbe, 0xd3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };
GUID Audit_ObjectAccess_FirewallConnection_ = { 0x0cce9226, 0x69ae, 0x11d9, { 0xbe, 0xd3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };

/*
    {0CCE9232-69AE-11D9-BED3-505054503030}
    Identifies the MPSSVC Rule-Level Policy Change audit subcategory.
    This subcategory audits events generated by changes in policy rules used by Windows Firewall.
*/

GUID Audit_ObjectAccess_FirewallRuleChange_ = { 0x0cce9232, 0x69ae, 0x11d9, { 0xbe, 0xd3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };

bool SetAuditPolicy(const GUID* pSubCategoryGuids, ULONG dwPolicyCount, uint32 AuditingMode, uint32* pOldAuditingMode)
{
	PAUDIT_POLICY_INFORMATION AuditPolicyInformation;
	BOOLEAN success = AuditQuerySystemPolicy(pSubCategoryGuids, dwPolicyCount, &AuditPolicyInformation);
	if (!success)
		return false;

	if ((AuditPolicyInformation->AuditingInformation & AuditingMode) != AuditingMode)
	{
		if (pOldAuditingMode) *pOldAuditingMode = AuditPolicyInformation->AuditingInformation;

		AuditPolicyInformation->AuditingInformation = AuditingMode;

		success = AuditSetSystemPolicy(AuditPolicyInformation, 1);
	}

	AuditFree(AuditPolicyInformation);
	return success;
}

uint32 GetAuditPolicy(const GUID* pSubCategoryGuids, ULONG dwPolicyCount)
{
    PAUDIT_POLICY_INFORMATION AuditPolicyInformation;
    BOOLEAN success = AuditQuerySystemPolicy(pSubCategoryGuids, dwPolicyCount, &AuditPolicyInformation);
    if (!success)
        return -1;

    uint32 AuditingMode = AuditPolicyInformation->AuditingInformation;

    AuditFree(AuditPolicyInformation);
    return AuditingMode;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities for custom Custom Event Logging

bool EventLogExists(const std::wstring& logName/*, const std::wstring& machineName*/)
{
    LONG result;
    HKEY hKeyMachine = nullptr;
    HKEY hKeyLog = nullptr;

    /*if (!machineName.empty() && machineName != L".")
    {
        result = RegConnectRegistry(machineName.c_str(), HKEY_LOCAL_MACHINE, &hKeyMachine);
        if (result != ERROR_SUCCESS)
            goto cleanup;
    }
    else*/
        hKeyMachine = HKEY_LOCAL_MACHINE;

    result = RegOpenKeyEx(hKeyMachine, L"SYSTEM\\CurrentControlSet\\Services\\EventLog", 0, KEY_READ, &hKeyLog);
    if (result == ERROR_SUCCESS)
    {
        result = RegOpenKeyEx(hKeyLog, logName.c_str(), 0, KEY_READ, &hKeyLog);

        RegCloseKey(hKeyLog);
    }

    return result == ERROR_SUCCESS;
}

static HKEY OpenEventSourceKey(const std::wstring& sourceName, bool bWrite = false)
{
    LONG result;
    HKEY hKeyLogs = nullptr;
    HKEY hKeyLog = nullptr;

    DWORD index = 0;
    WCHAR subkeyName[255];
    DWORD subkeyNameSize = sizeof(subkeyName);

    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog", 0, KEY_READ, &hKeyLogs);
    if (result != ERROR_SUCCESS)
        goto cleanup;

    // Enumerate subkeys under HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog
    while (RegEnumKeyEx(hKeyLogs, index, subkeyName, &subkeyNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
        // Open each subkey and check if it has a subkey with the name sourceName
        result = RegOpenKeyEx(hKeyLogs, subkeyName, 0, KEY_READ, &hKeyLog);
        if (result != ERROR_SUCCESS)
        {
            // Handle error if necessary
            continue; // Try the next subkey
        }

        // Check if the subkey has the sourceName subsubkey
        HKEY hKeyLogSource = nullptr;
        result = RegOpenKeyEx(hKeyLog, sourceName.c_str(), 0, bWrite ? KEY_WRITE : KEY_READ, &hKeyLogSource);
        if (result == ERROR_SUCCESS)
        {
            // Found the sourceName subsubkey, so it exists
            RegCloseKey(hKeyLog);
            RegCloseKey(hKeyLogs);
            return hKeyLogSource;
        }

        RegCloseKey(hKeyLog);
        subkeyNameSize = sizeof(subkeyName); // Reset subkeyNameSize for the next iteration
        index++;
    }

cleanup:
    if (hKeyLogs != nullptr)
        RegCloseKey(hKeyLogs);
    return NULL;
}

bool EventSourceExists(const std::wstring& sourceName)
{
    HKEY hKeyLogSource = OpenEventSourceKey(sourceName);
    if(!hKeyLogSource)
        return false;
    RegCloseKey(hKeyLogSource);
    return true;
}

bool CreateEventSource(const std::wstring& sourceName, std::wstring logName)
{
    LONG result;
    HKEY hEventLogKey = nullptr;
    HKEY hEventLogLogKey = nullptr;
    HKEY hLogSourceKey = nullptr;

    if (logName.empty())
        logName = sourceName;

    // Set the EventMessageFile value to specify the message DLL or EXE
    std::wstring messageFile = L"C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\EventLogMessages.dll"; // Replace with the actual path to your message file // todo
    // Set the TypesSupported value (for example, 7 specifies error, warning, and information event types)
    //DWORD typesSupported = 7; // Modify as needed

    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog", 0, KEY_WRITE, &hEventLogKey);
    if (result == ERROR_SUCCESS)
    {
        //L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\[logName]"
        result = RegCreateKeyEx(hEventLogKey, logName.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hEventLogLogKey, nullptr);
        if (result == ERROR_SUCCESS)
        {
            //L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\[logName]\\[sourceName]"
            result = RegCreateKeyEx(hEventLogLogKey, sourceName.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hLogSourceKey, nullptr);
            if (result == ERROR_SUCCESS)
            {
                result = RegSetValueEx(hLogSourceKey, L"EventMessageFile", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(messageFile.c_str()), static_cast<DWORD>((messageFile.length() + 1) * sizeof(wchar_t)));

                //result = RegSetValueEx(hLogSourceKey, L"TypesSupported", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&typesSupported), sizeof(typesSupported));                

                RegCloseKey(hLogSourceKey);
            }

            RegCloseKey(hEventLogLogKey);
        }
        
        RegCloseKey(hEventLogKey);
    }

    return result == ERROR_SUCCESS;
}

bool EmptyEventLog(const std::wstring& logName)
{
    HANDLE hEventLog = OpenEventLog(nullptr, logName.c_str());
    if (hEventLog == nullptr)
        return false;

    bool bOk = !!ClearEventLog(hEventLog, nullptr);

    CloseEventLog(hEventLog);

    return bOk;
}

bool DeleteEventLog(const std::wstring& logName)
{
    const std::wstring eventLogKey = L"SYSTEM\\CurrentControlSet\\Services\\EventLog";
    HKEY hEventLogKey = nullptr;

    LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, eventLogKey.c_str(), 0, KEY_WRITE, &hEventLogKey);
    if (result == ERROR_SUCCESS)
    {
        result = RegDeleteTree(hEventLogKey, logName.c_str());

        RegCloseKey(hEventLogKey);
    }

    return result == ERROR_SUCCESS;
}

bool DeleteEventSource(const std::wstring& sourceName)
{
    LONG result = ERROR_OBJECT_NOT_FOUND;

    HKEY hKeyLogSource = OpenEventSourceKey(sourceName);
    if (!hKeyLogSource)
    {
        result = RegDeleteTree(hKeyLogSource, NULL);

        RegCloseKey(hKeyLogSource);
    }

    return result == ERROR_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CEventLogListener

CEventLogListener::CEventLogListener()
{
	m_hSubscription = NULL;
}

CEventLogListener::~CEventLogListener()
{
    if (m_hSubscription) 
        EvtClose(m_hSubscription);
}

DWORD WINAPI CEventLogListener::Callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) 
{
	CEventLogListener* This = (CEventLogListener*)pContext;

	This->OnEvent(hEvent);

	return 0;
}

bool CEventLogListener::Start(const std::wstring& XmlQuery)
{
	if (m_hSubscription)
		return true;

	m_hSubscription = EvtSubscribe(NULL, NULL, L"", XmlQuery.c_str(), NULL, this, Callback, EvtSubscribeToFutureEvents);
    if (m_hSubscription == NULL)
        false;
	return true;
}

void CEventLogListener::Stop()
{
	if (m_hSubscription) {
		EvtClose(m_hSubscription);
		m_hSubscription = NULL;
	}
}

std::wstring CEventLogListener::GetEventXml(EVT_HANDLE hEvent)
{
    std::wstring buffer;
    buffer.resize(0x1000);
    DWORD usedSize = 0;
    DWORD propCount = 0;
    while (!EvtRender(NULL, hEvent, EvtRenderEventXml, (DWORD)buffer.size(), (PVOID)buffer.c_str(), &usedSize, &propCount))
    {
        DWORD error = GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER || buffer.size() > usedSize) {
            usedSize = 0;
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    buffer.resize(usedSize);
    return std::move(buffer);
}

std::vector<BYTE> CEventLogListener::GetEventData(EVT_HANDLE hEvent, DWORD ValuePathsCount, LPCWSTR* ValuePaths)
{
    std::vector<BYTE> buffer;

	EVT_HANDLE renderContext = EvtCreateRenderContext(ValuePathsCount, ValuePaths, EvtRenderContextValues);
	//EVT_HANDLE renderContext = EvtCreateRenderContext(0, NULL , EvtRenderContextSystem);
	//EVT_HANDLE renderContext = EvtCreateRenderContext(0, NULL , EvtRenderContextUser);
	if (renderContext == NULL)
	{
		/*
		wchar_t* lpMsgBuf;
		wchar_t* lpDisplayBuf;
		DWORD dw = GetLastError(); 
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
		DbgPrint("%S\n", lpMsgBuf);
		LocalFree(lpMsgBuf);
		LocalFree(lpDisplayBuf);
		*/
		return std::vector<BYTE>();
	}

    buffer.resize(0x1000);
    DWORD usedSize = 0;
    DWORD propCount = 0;
    while (!EvtRender(renderContext, hEvent, EvtRenderEventValues, (DWORD)buffer.size(), buffer.data(), &usedSize, &propCount))
    {
        DWORD error = GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER || buffer.size() > usedSize) {
            usedSize = 0;
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    buffer.resize(usedSize);

    EvtClose(renderContext);

    return std::move(buffer);
}

LIBRARY_EXPORT std::wstring charArrayToWString(const char* charArray);

std::wstring CEventLogListener::GetWinVariantString(const EVT_VARIANT& value, const std::wstring& defValue)
{
	if (value.Type == EvtVarTypeString && value.StringVal != NULL)
		return value.StringVal;
	if (value.Type == EvtVarTypeAnsiString && value.StringVal != NULL)
		return charArrayToWString(value.AnsiStringVal);
	return defValue;
}

//CVariant CEventLogListener::GetEvtVariant(const struct _EVT_VARIANT& value, const CVariant& defValue)
//{
//	switch (value.Type)
//	{
//	case EvtVarTypeBoolean:	return value.BooleanVal;
//
//	case EvtVarTypeSByte:	return value.SByteVal;
//    case EvtVarTypeByte:	return value.ByteVal;
//    case EvtVarTypeInt16:	return value.Int16Val;
//    case EvtVarTypeUInt16:	return value.UInt16Val;
//    case EvtVarTypeInt32:	return value.Int32Val;
//    case EvtVarTypeUInt32:	return value.UInt32Val;
//    case EvtVarTypeInt64:	return value.Int64Val;
//    case EvtVarTypeUInt64:	return value.UInt64Val;
//
//	case EvtVarTypeFileTime:return value.FileTimeVal; // UInt64Val
//
//    case EvtVarTypeSingle:	return value.SingleVal;
//    case EvtVarTypeDouble:	return value.DoubleVal;
//	}
//    return defValue;
//}


////////////////////////////////////////////////////////////////////////////////////////////////////////
// SEventLogDll

struct SEventLogDll 
{
    SEventLogDll()
    {
        //
        // avoid static dependencies wherever possible
        //

        hWEvtApi = LoadLibraryW(L"wevtapi.dll");
        if (!hWEvtApi)
            return;
        
        *(PVOID*)&pEvtSubscribe = GetProcAddress(hWEvtApi, "EvtSubscribe");

        *(PVOID*)&pEvtCreateRenderContext = GetProcAddress(hWEvtApi, "EvtCreateRenderContext");
        *(PVOID*)&pEvtRender = GetProcAddress(hWEvtApi, "EvtRender");

        *(PVOID*)&pEvtQuery = GetProcAddress(hWEvtApi, "EvtQuery");
        *(PVOID*)&pEvtNext = GetProcAddress(hWEvtApi, "EvtNext");
        *(PVOID*)&pEvtSeek = GetProcAddress(hWEvtApi, "EvtSeek");

        *(PVOID*)&pEvtClose = GetProcAddress(hWEvtApi, "EvtClose");
    }

	HMODULE hWEvtApi = NULL;
	EVT_HANDLE(WINAPI* pEvtSubscribe)(EVT_HANDLE Session, HANDLE SignalEvent, LPCWSTR ChannelPath, LPCWSTR Query, EVT_HANDLE Bookmark, PVOID Context, EVT_SUBSCRIBE_CALLBACK Callback, DWORD Flags) = NULL;
    EVT_HANDLE (WINAPI* pEvtQuery)(EVT_HANDLE Session, LPCWSTR Path, LPCWSTR Query, DWORD Flags);
	BOOL (WINAPI* pEvtNext)(EVT_HANDLE ResultSet, DWORD EventsSize, PEVT_HANDLE Events, DWORD Timeout, DWORD Flags, PDWORD Returned);
	BOOL (WINAPI* pEvtSeek)(EVT_HANDLE ResultSet, LONGLONG Position, EVT_HANDLE Bookmark, DWORD Timeout, DWORD Flags);
	EVT_HANDLE(WINAPI* pEvtCreateRenderContext)(DWORD ValuePathsCount, LPCWSTR* ValuePaths, DWORD Flags) = NULL;
	BOOL(WINAPI* pEvtRender)(EVT_HANDLE Context, EVT_HANDLE Fragment, DWORD Flags, DWORD BufferSize, PVOID Buffer, PDWORD BufferUsed, PDWORD PropertyCount) = NULL;
	BOOL(WINAPI* pEvtClose)(EVT_HANDLE Object) = NULL;

} *CEventLogListener::m_Dll = NULL;

SEventLogDll* CEventLogListener::Dll()
{
    if (!m_Dll) // todo initonce
        m_Dll = new SEventLogDll();
    return m_Dll;
}

EVT_HANDLE CEventLogListener::EvtSubscribe(EVT_HANDLE Session, HANDLE SignalEvent, LPCWSTR ChannelPath, LPCWSTR Query, EVT_HANDLE Bookmark, PVOID Context, EVT_SUBSCRIBE_CALLBACK Callback, DWORD Flags)
{
    auto func = Dll()->pEvtSubscribe;
    if (!func) return NULL;
    return func(Session, SignalEvent, ChannelPath, Query, Bookmark, Context, Callback, Flags);
}

EVT_HANDLE CEventLogListener::EvtCreateRenderContext(DWORD ValuePathsCount, LPCWSTR* ValuePaths, DWORD Flags)
{
    auto func = Dll()->pEvtCreateRenderContext;
    if (!func) return NULL;
    return func(ValuePathsCount, ValuePaths, Flags);
}

BOOL CEventLogListener::EvtRender(EVT_HANDLE Context, EVT_HANDLE Fragment, DWORD Flags, DWORD BufferSize, PVOID Buffer, PDWORD BufferUsed, PDWORD PropertyCount)
{
    auto func = Dll()->pEvtRender;
    if (!func) return FALSE;
    return func(Context, Fragment, Flags, BufferSize, Buffer, BufferUsed, PropertyCount);
}

EVT_HANDLE CEventLogListener::EvtQuery(EVT_HANDLE Session, LPCWSTR Path, LPCWSTR Query, DWORD Flags)
{
    auto func = Dll()->pEvtQuery;
    if (!func) return NULL;
    return func(Session, Path, Query, Flags);
}

BOOL CEventLogListener::EvtNext(EVT_HANDLE ResultSet, DWORD EventsSize, PEVT_HANDLE Events, DWORD Timeout, DWORD Flags, PDWORD Returned)
{
    auto func = Dll()->pEvtNext;
    if (!func) return FALSE;
    return func(ResultSet, EventsSize, Events, Timeout, Flags, Returned);
}

BOOL CEventLogListener::EvtSeek(EVT_HANDLE ResultSet, LONGLONG Position, EVT_HANDLE Bookmark, DWORD Timeout, DWORD Flags)
{
    auto func = Dll()->pEvtSeek;
    if (!func) return FALSE;
    return func(ResultSet, Position, Bookmark, Timeout, Flags);
}

BOOL CEventLogListener::EvtClose(EVT_HANDLE Object)
{
    auto func = Dll()->pEvtClose;
    if (!func) return FALSE;
    return func(Object);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
// CEventLogger

CEventLogger::CEventLogger(const wchar_t* name)
{
    m_Handle = RegisterEventSourceW(NULL, name);
}

CEventLogger::~CEventLogger()
{
    DeregisterEventSource(m_Handle);
}

void CEventLogger::LogEvent(WORD wType, WORD wCategory, DWORD dwEventID, const std::wstring& string, const CBuffer* pData)
{
    std::vector<const wchar_t*> strings;
    strings.push_back(string.c_str());
    LogEvent(wType, wCategory, dwEventID, strings, pData);
}

void CEventLogger::LogEvent(WORD wType, WORD wCategory, DWORD dwEventID, const std::vector<const wchar_t*>& strings, const CBuffer* pData)
{
    ReportEventW(m_Handle, wType, wCategory, dwEventID, NULL, (WORD)strings.size(), pData ? (DWORD)pData->GetSize() : 0, (LPCWSTR*)strings.data(), pData ? (LPVOID)pData->GetBuffer() : NULL);
}

void CEventLogger::ClearLog(const wchar_t* name)
{
    EmptyEventLog(name);
}