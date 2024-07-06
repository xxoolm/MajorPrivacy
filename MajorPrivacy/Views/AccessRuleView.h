#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/AccessRuleModel.h"

class CAccessRuleView : public CPanelViewEx<CAccessRuleModel>
{
	Q_OBJECT

public:
	CAccessRuleView(QWidget *parent = 0);
	virtual ~CAccessRuleView();

	void					Sync(const QList<CAccessRulePtr>& RuleList);

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenRullDialog(const CAccessRulePtr& pRule);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnRuleAction();

private:

	QAction*				m_pCreateRule;
	QAction*				m_pEnableRule;
	QAction*				m_pDisableRule;
	QAction*				m_pRemoveRule;
	QAction*				m_pEditRule;
	QAction*				m_pCloneRule;
};
