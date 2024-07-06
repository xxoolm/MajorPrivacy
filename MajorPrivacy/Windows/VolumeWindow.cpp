#include "pch.h"
#include "VolumeWindow.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Common.h"
#include <QStorageInfo>


CVolumeWindow::CVolumeWindow(EAction Action, QWidget *parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	//flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	this->setWindowTitle(tr("MajorPrivacy - Password Entry"));

	m_Action = Action;

	connect(ui.chkShow, SIGNAL(clicked(bool)), this, SLOT(OnShowPassword()));

	connect(ui.btnMount, SIGNAL(clicked()), this, SLOT(BrowseMountPoint()));
	connect(ui.txtImageSize, SIGNAL(textChanged(const QString&)), this, SLOT(OnImageSize()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(CheckPassword()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	switch (m_Action)
	{
	case eSetPW:
		ui.lblInfo->setText(tr("Enter new config password"));
		ui.lblIcon->setPixmap(QPixmap::fromImage(QImage(":/Actions/LockClosed.png")));
		break;
	case eNew:
		ui.lblInfo->setText(tr("Creating new box image, please enter a secure password, and choose a disk image size."));
		ui.lblIcon->setPixmap(QPixmap::fromImage(QImage(":/Actions/LockClosed.png")));
		break;
	case eGetPW:
	case eMount:
		ui.lblInfo->setText(tr("Enter Box Image password:"));
		ui.lblIcon->setPixmap(QPixmap::fromImage(QImage(":/Actions/LockOpen.png")));
		break;
	case eChange:
		ui.lblInfo->setText(tr("Enter Box Image passwords:"));
		ui.lblIcon->setPixmap(QPixmap::fromImage(QImage(":/Actions/LockClosed.png")));
		break;
	}


	if (m_Action == eNew || m_Action == eSetPW) 
	{
		ui.lblPassword->setVisible(false);
		ui.txtPassword->setVisible(false);

		ui.txtNewPassword->setFocus();
	}
	else 
		ui.txtPassword->setFocus();

	if (m_Action == eNew)
	{
		ui.txtImageSize->setText(QString::number(2 * 1024 * 1024)); // suggest 2GB
	}

	if (m_Action != eNew)
	{
		ui.lblImageSize->setVisible(false);
		ui.txtImageSize->setVisible(false);
		ui.lblImageSizeKb->setVisible(false);
	}

	if (m_Action == eMount || m_Action == eGetPW)
	{
		ui.lblNewPassword->setVisible(false);
		ui.txtNewPassword->setVisible(false);
		ui.lblRepeatPassword->setVisible(false);
		ui.txtRepeatPassword->setVisible(false);
	}

	if (m_Action == eMount)
	{
		QList<char> usedDriveLetters;
		QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();
		for (const QStorageInfo &drive : drives) {
			QString driveLetter = drive.rootPath();
			if (!driveLetter.isEmpty() && driveLetter.length() >= 2 && driveLetter.at(1) == ':')
				usedDriveLetters.append(driveLetter.at(0).toUpper().toLatin1());
		}

		for (char letter = 'D'; letter <= 'Z'; ++letter) {
			if (!usedDriveLetters.contains(letter))
				ui.cmbMount->addItem(QString(1, letter) + ":\\");
		}
	}
	else
	{
		ui.lblMount->setVisible(false);
		ui.cmbMount->setVisible(false);
		ui.btnMount->setVisible(false);
	}

	//if (!bNew) {
		ui.lblCipher->setVisible(false);
		ui.cmbCipher->setVisible(false);
	//}
	ui.cmbCipher->addItem("AES", 0);
	ui.cmbCipher->addItem("Twofish", 1);
	ui.cmbCipher->addItem("Serpent", 2);
	ui.cmbCipher->addItem("AES-Twofish", 3);
	ui.cmbCipher->addItem("Twofish-Serpent", 4);
	ui.cmbCipher->addItem("Serpent-AES", 5);
	ui.cmbCipher->addItem("AES-Twofish-Serpent", 6);

	//if (m_Action != eMount)
	ui.chkProtect->setVisible(false); // todo

	//restoreGeometry(theConf->GetBlob("VolumeWindow/Window_Geometry"));
}

CVolumeWindow::~CVolumeWindow()
{
	//theConf->SetBlob("VolumeWindow/Window_Geometry", saveGeometry());
}

void CVolumeWindow::OnShowPassword()
{
	ui.txtPassword->setEchoMode(ui.chkShow->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
	ui.txtNewPassword->setEchoMode(ui.chkShow->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
	ui.txtRepeatPassword->setEchoMode(ui.chkShow->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
}

void CVolumeWindow::OnImageSize()
{
	ui.lblImageSizeKb->setText(tr("kilobytes (%1)").arg(FormatSize(GetImageSize())));
}

void CVolumeWindow::CheckPassword()
{
	if (m_Action == eMount || m_Action == eGetPW) {
		m_Password = ui.txtPassword->text();
	}
	else {

		if (ui.txtNewPassword->text() != ui.txtRepeatPassword->text()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Passwords don't match!!!"));
			return;
		}
		if (ui.txtNewPassword->text().length() < 20) {
			if (QMessageBox::warning(this, "MajorPrivacy", tr("WARNING: Short passwords are easy to crack using brute force techniques!\n\n"
				"It is recommended to choose a password consisting of 20 or more characters. Are you sure you want to use a short password?")
				, QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
				return;
		}
		if (ui.txtNewPassword->text().length() > 128) {
			QMessageBox::warning(this, "MajorPrivacy", tr("The password is constrained to a maximum length of 128 characters. \n"
				"This length permits approximately 384 bits of entropy with a passphrase composed of actual English words, \n"
				"increases to 512 bits with the application of Leet (L337) speak modifications, and exceeds 768 bits when composed of entirely random printable ASCII characters.")
				, QMessageBox::Ok);
			return;
		}

		if (m_Action == eNew || m_Action == eSetPW)
			m_Password = ui.txtNewPassword->text();
		else if (m_Action == eChange) {
			m_Password = ui.txtPassword->text();
			m_NewPassword = ui.txtNewPassword->text();
		}
	}
	
	if (m_Action == eNew) {
		if (GetImageSize() < 128 * 1024 * 1024) { // ask for 256 mb but silently accept >= 128 mb
			QMessageBox::critical(this, "MajorPrivacy", tr("The Box Disk Image must be at least 256 MB in size, 2GB are recommended."));
			SetImageSize(256 * 1024 * 1024);
			return;
		}
	}

	accept();
}

void CVolumeWindow::BrowseMountPoint()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Select Mount Point"), ui.cmbMount->currentText());
	if (!path.isEmpty())
		ui.cmbMount->setCurrentText(path);
}