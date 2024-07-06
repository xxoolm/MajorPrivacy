#include "pch.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "HomePage.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/EvtUtil.h"
#include "../../Library/Helpers/Scoped.h"
#include "../MiscHelpers/Common/CustomStyles.h"


CHomePage::CHomePage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	// Event Log
	m_pEventLog = new QTreeWidgetEx();
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pEventLog->setStyle(pStyle);
	m_pEventLog->setItemDelegate(new CTreeItemDelegate());
	//m_pEventLog->setHeaderLabels(tr("#|Symbol|Stack address|Frame address|Control address|Return address|Stack parameters|File info").split("|"));
	m_pEventLog->setHeaderLabels(tr("Time Stamp|Type|Class|Event|Message").split("|"));
	m_pEventLog->setMinimumHeight(50);

	m_pEventLog->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pEventLog->setSortingEnabled(false);

	m_pEventLog->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pEventLog, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(CFinder::AddFinder(m_pEventLog, this, true, &m_pFinder));
	// 

	m_pMainLayout->addWidget(m_pEventLog);

	std::wstring XmlQuery = 
		L"<QueryList>"
		L"  <Query Id=\"0\" Path=\"" APP_NAME L"\">"
		L"    <Select Path=\"" APP_NAME L"\">*</Select>"
		L"  </Query>"
		L"</QueryList>";

	LoadLog(XmlQuery);

	m_pEventListener = new CEventLogCallback([](PVOID pParam, EVT_HANDLE hEvent) {
		AddLogEventFunc((CHomePage*)pParam, hEvent);
	}, this);

	m_pEventListener->Start(XmlQuery);
}

CHomePage::~CHomePage()
{
	m_pEventListener->Stop();
	delete m_pEventListener;
}

void CHomePage::AddLogEventFunc(CHomePage* This, HANDLE hEvent)
{
	//auto test = CEventLogCallback::GetEventXml(hEvent);
/*
<Event
	xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>
	<System>
		<Provider Name='MajorPrivacy'/>
		<EventID Qualifiers='0'>222</EventID>
		<Version>0</Version>
		<Level>4</Level>
		<Task>111</Task>
		<Opcode>0</Opcode>
		<Keywords>0x80000000000000</Keywords>
		<TimeCreated SystemTime='2023-09-25T19:29:21.5293474Z'/>
		<EventRecordID>28</EventRecordID>
		<Correlation/>
		<Execution ProcessID='0' ThreadID='0'/>
		<Channel>MajorPrivacy</Channel>
		<Computer>dev2019</Computer>
		<Security/>
	</System>
	<EventData>
		<Data>I'm in an event log BAM!!!!</Data>
		<Data>Test 123</Data>
		<Binary>74657374203132333400</Binary>
	</EventData>
</Event>
*/

	static PCWSTR eventProperties[] = {
		L"Event/System/Provider/@Name",			// 0

		L"Event/System/Level",					// 1	// wType
		L"Event/System/Task",					// 3	// wCategory
		L"Event/System/EventID",				// 2	// dwEventID

		L"Event/EventData/Data",				// 4	// lpStrings
		L"Event/EventData/Binary",				// 5	// lpRawData

		L"Event/System/TimeCreated/@SystemTime",// 6
	};

	std::vector<BYTE> data = CEventLogCallback::GetEventData(hEvent, ARRAYSIZE(eventProperties), eventProperties);

	PEVT_VARIANT values = (PEVT_VARIANT)data.data();

	//PEVT_VARIANT value0 = &values[0];	// EvtVarTypeString

	PEVT_VARIANT vType  = &values[1];	// EvtVarTypeByte
	PEVT_VARIANT vClass = &values[2];	// EvtVarTypeUInt16
	PEVT_VARIANT vEvent = &values[3];	// EvtVarTypeUInt16

	PEVT_VARIANT vText  = &values[4];	// EvtVarTypeString | EVT_VARIANT_TYPE_ARRAY	
	
	QStringList Message;
	if (vText->Type & EVT_VARIANT_TYPE_ARRAY) {
		for (DWORD i = 0; i < vText->Count; i++)
			Message.append(QString::fromStdWString(vText->StringArr[i]));
	} else
		Message.append(QString::fromStdWString(values[4].StringVal));

	PBYTE        vData  =  values[5].BinaryVal;	// EvtVarTypeBinary

	PEVT_VARIANT uTime  = &values[6]; // EvtVarTypeFileTime

	QDateTime TimeStamp = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(CEventLogListener::GetWinVariantNumber<quint64>(*uTime, 0)));

	QMetaObject::invokeMethod(This, "AddLogEvent", Qt::QueuedConnection, 
		Q_ARG(QDateTime, TimeStamp),
		Q_ARG(int, CEventLogListener::GetWinVariantNumber<int>(*vType, 0)), 
		Q_ARG(int, CEventLogListener::GetWinVariantNumber<int>(*vClass, 0)), 
		Q_ARG(int, CEventLogListener::GetWinVariantNumber<int>(*vEvent, 0)), 
		Q_ARG(QString, Message.join(" ")));
}

void CHomePage::AddLogEvent(const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message)
{
	QTreeWidgetItem* pItem = new QTreeWidgetItem();

	pItem->setText(0, TimeStamp.toString("dd.MM.yyyy hh:mm:ss"));
	pItem->setText(1, QString::number(Type));
	pItem->setText(2, QString::number(Class));
	pItem->setText(3, QString::number(Event));
	pItem->setText(4, Message);

	m_pEventLog->insertTopLevelItem(0, pItem);
}

void CHomePage::LoadLog(const std::wstring& XmlQuery)
{
	QList<QTreeWidgetItem*> Items;

	QByteArray buff;
	buff.resize(8 * 1024);
	CScopedHandle hEventLog(OpenEventLogW(NULL, APP_NAME), CloseEventLog);
	if (!hEventLog) return;
    for (;;) 
	{
        ULONG bytesRead, bytesNeeded;
        if(!ReadEventLogW(hEventLog, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, buff.data(), buff.size(), &bytesRead, &bytesNeeded))
            break;

        EVENTLOGRECORD *rec = (EVENTLOGRECORD *)buff.data();
        while (bytesRead > 0) 
		{
			//rec->DataLength;
			//rec->DataOffset;

            //WCHAR *source = (WCHAR *) ((UCHAR *)rec + sizeof(EVENTLOGRECORD));			

			QStringList Message;
			WCHAR* str = (WCHAR *)((UCHAR *)rec + rec->StringOffset);
			for (int i = 0; i < rec->NumStrings; i++) {
				size_t len = wcslen(str);
				Message.append(QString::fromWCharArray(str, len));
				str += len + 1;
			}

			QTreeWidgetItem* pItem = new QTreeWidgetItem();

			pItem->setText(0, QDateTime::fromSecsSinceEpoch(rec->TimeGenerated).toString("dd.MM.yyyy hh:mm:ss"));
			pItem->setText(1, QString::number(rec->EventType));
			pItem->setText(2, QString::number(rec->EventCategory));
			pItem->setText(3, QString::number(rec->EventID));
			pItem->setText(4, Message.join(" "));

			Items.prepend(pItem);

            bytesRead -= rec->Length;
            rec = (EVENTLOGRECORD *)((UCHAR *)rec + rec->Length);
        }
    }

	m_pEventLog->insertTopLevelItems(0, Items);

    /*EVT_HANDLE hQuery = CEventLogCallback::EvtQuery(NULL, NULL, XmlQuery.c_str(), EvtQueryChannelPath);
	if (hQuery) 
	{
		for(;;)
		{
			EVT_HANDLE hEvent[100];

			DWORD count = 0;
			if (!CEventLogCallback::EvtNext(hQuery, ARRAYSIZE(hEvent), hEvent, 0, 0, &count)) 
			{
				DWORD status = GetLastError();
				if (status == ERROR_NO_MORE_ITEMS)
					break;
				//else {
				//	// error
				//	break;
				//}
			}

			for (DWORD i = 0; i < count; i++)
			{
				//auto test = CEventLogCallback::GetEventXml(hEvent[i]);
				AddLogEvent(this, hEvent[i]);
				CEventLogCallback::EvtClose(hEvent[i]);
			}
		}

		CEventLogCallback::EvtClose(hQuery);
	}*/
}
