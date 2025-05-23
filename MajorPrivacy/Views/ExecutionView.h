#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ExecutionModel.h"
#include "../Core/Programs/WindowsService.h"

class CExecutionView : public CPanelViewEx<CExecutionModel>
{
	Q_OBJECT

public:
	CExecutionView(QWidget *parent = 0);
	virtual ~CExecutionView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid = QString());

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnCleanUpDone();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbRole;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnAll;
	QToolButton*			m_pBtnExpand;

	QSet<CProgramFilePtr>					m_CurPrograms;
	QSet<CWindowsServicePtr>				m_CurServices;
	QFlexGuid								m_CurEnclaveGuid;
	QMap<SExecutionKey, SExecutionItemPtr>	m_ParentMap;
	QMap<SExecutionKey, SExecutionItemPtr>	m_ExecutionMap;
	qint32									m_FilterRole = 0;
	qint32									m_FilterAction = 0;
	bool									m_FilterAll = false;	
	quint64									m_RecentLimit = 0;
};
