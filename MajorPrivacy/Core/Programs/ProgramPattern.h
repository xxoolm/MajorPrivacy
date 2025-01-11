#pragma once
#include "ProgramGroup.h"

class CProgramPattern: public CProgramList
{
	Q_OBJECT
public:
	CProgramPattern(QObject* parent = nullptr);

	virtual EProgramType GetType() const		{ return EProgramType::eFilePattern; }

	virtual QIcon DefaultIcon() const;

	void SetPattern(const QString& Pattern);
	QString GetPattern() const					{ return m_Pattern; }
	//bool MatchFileName(const QString& FileName);

	virtual QString GetPath() const				{ return m_Pattern; }
	
protected:

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QString m_Pattern;
	//QRegularExpression m_RegExp;
};


typedef QSharedPointer<CProgramPattern> CProgramPatternPtr;