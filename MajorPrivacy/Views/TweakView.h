#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/TweakModel.h"
#include "../MiscHelpers/Common/CustomStyles.h"

class CTreeItemDelegate2 : public CTreeItemDelegate
{
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		if (!index.parent().isValid()) { // root item
			size.setHeight(32);
		} else {
			size.setHeight(24);
		}
		return size;
	}
};

class CTweakView : public CPanelViewEx<CTweakModel>
{
	Q_OBJECT

public:
	CTweakView(QWidget *parent = 0);
	virtual ~CTweakView();

	void					Sync(const CTweakPtr& pRoot);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnRefresh();
	void					OnTweaksChanged();

	void					OnApprove();
	void					OnRestore();

	void					OnCheckChanged(const QModelIndex& Index, bool State);

protected:
	CTweakPtr				m_pRoot;

private:

	QToolBar*				m_pToolBar = nullptr;
	QToolButton*			m_pBtnRefresh = nullptr;
	QToolButton*			m_pBtnHidden = nullptr;
	QToolButton*			m_pBtnApprove = nullptr;
	QToolButton*			m_pBtnRestore = nullptr;
	QToolButton*			m_pBtnExpand = nullptr;

	quint64					m_uRefreshPending = 0;
};
