#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ProgramWnd.h"
#include "../Core/Programs/ProgramItem.h"


class CProgramWnd : public QDialog
{
	Q_OBJECT

public:
	CProgramWnd(CProgramItemPtr pProgram, QWidget *parent = Q_NULLPTR);
	~CProgramWnd();

signals:
	void Closed();

private slots:
	void OnSaveAndClose();

protected:
	void closeEvent(QCloseEvent* e);

	CProgramItemPtr m_pProgram;

private:
	Ui::ProgramWnd ui;

};
