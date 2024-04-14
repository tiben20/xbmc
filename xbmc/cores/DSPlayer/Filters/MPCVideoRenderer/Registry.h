

#ifndef __REGISTRY_H__
#define __REGISTRY_H__
#include "Utils/StdString.h"
#include "DSUtil/Geometry.h"

class CRegistry
{
public:
	CRegistry();
	~CRegistry();

int m_nLastError;

// CRegistry properties	
protected:
	HKEY m_hRootKey;
	BOOL m_bLazyWrite;
	CStdString m_strCurrentPath;

public:
	inline BOOL PathIsValid() {
		return (m_strCurrentPath.GetLength() > 0); }
	inline CStdString GetCurrentPath() {
		return m_strCurrentPath; }
	inline HKEY GetRootKey() {
		return m_hRootKey; }


//CRegistry	methods
public:
	BOOL ClearKey();
	BOOL SetRootKey(HKEY hRootKey);
	BOOL CreateKey(CStdString strKey);
	BOOL DeleteKey(CStdString strKey);
	BOOL DeleteValue(CStdString strName);
	int GetDataSize(CStdString strValueName);
	DWORD GetDataType(CStdString strValueName);
	int GetSubKeyCount();
	int GetValueCount();
	BOOL KeyExists(CStdString strKey, HKEY hRootKey = NULL);
	BOOL SetKey(CStdString strKey, BOOL bCanCreate);
	BOOL ValueExists(CStdString strName);
	void RenameValue(CStdString strOldName, CStdString strNewName);

	// data reading functions
	//COleDateTime ReadDateTime(CStdString strName, COleDateTime dtDefault);
	double ReadFloat(CStdString strName, double fDefault);
	CStdString ReadString(CStdString strName, CStdString strDefault);
	int ReadInt(CStdString strName, int nDefault);
	BOOL ReadBool(CStdString strName, BOOL bDefault);
	COLORREF ReadColor(CStdString strName, COLORREF rgbDefault);
	//BOOL ReadFont(CStdString strName, CFont* pFont);
	BOOL ReadPoint(CStdString strName, Com::SmartPoint* pPoint);
	BOOL ReadSize(CStdString strName, Com::SmartSize* pSize);
	BOOL ReadRect(CStdString strName, Com::SmartRect* pRect);
	BOOL ReadDword(CStdString strName, DWORD& dwDefault);

	// data writing functions
	BOOL WriteBool(CStdString strName, BOOL bValue);
	//BOOL WriteDateTime(CStdString strName, COleDateTime dtValue);
	BOOL WriteString(CStdString strName, CStdString strValue);
	BOOL WriteFloat(CStdString strName, double fValue);
	BOOL WriteInt(CStdString strName, int nValue);
	BOOL WriteColor(CStdString strName, COLORREF rgbValue);
	//BOOL WriteFont(CStdString strName, CFont* pFont);
	BOOL WritePoint(CStdString strName, Com::SmartPoint* pPoint);
	BOOL WriteSize(CStdString strName, Com::SmartSize* pSize);
	BOOL WriteRect(CStdString strName, Com::SmartRect* pRect);
	BOOL WriteDword(CStdString strName, DWORD dwValue);

};// end of CRegistry class definition


#endif