// winLoger.cpp : Defines the entry point for the application.

#include "stdafx.h"
#include "shlwapi.h"

#include <vector>
#include <fstream> // работа с файлами
#include <iostream>
//#include <ifstream> // работа с файлами
#include <iomanip> // манипуляторы ввода/вывода
#include <map>

// trdm 2020-07-02 16:17:51 
#include <psapi.h> // << процессы 
#include <TCHAR.h> // Для _tcslwr

#include <stdio.h>
//#include "stringU.cpp"
#include <algorithm>
#include <list>
#include <WinBase.h> // GetVolumePathName
#include "winLoger.h" // GetVolumePathName

using namespace std;

UINT gTimerId = 0; // присобачим таймер: https://github.com/newleon/cpp_console_timer/blob/master/main.cpp
const int gTimerInterval = 10; // 10 секунд

// trdm 2021/11/04 15:26
#if _DEBUG
	const int gFsCheckInterval = 20; // временно
#else
	// в конце концов сканирование процессов не особо нагружающая операция, а файлы будут сканироваться только 
	// если присутствуют процессы....
	const int gFsCheckInterval = 120; // 2 минуты. 3 минуты. 
#endif

int gFsBalance = 0;//gFsCheckInterval; // balance - остаток средств
SYSTEMTIME gLastSt;
bool gDebug = true;
bool gLogIn_WndClassAndHwnd = false;		// log_in_wnd_class_name_and_hwnd = yes
bool gLogIn_WndClassAndHwnd_debug = false; // log_in_wnd_class_name_and_hwnd_debug = no
long gLastDataLogFile = 0;
long gScanCounter = 0;

DWORD gLastRunWindowsCheck = 0;

int		gCurMessages = 0;
string gLogFileName = "";	 // просто строка для переключения между gLogFileNameWrk и gLogFileNameAll
string gLogFileNameWrk = ""; // Лог для работы и калькуляции времени.
string gLogFileNameAll = ""; // Общий лог, все туда
string gLogFileNameWnd = ""; // лог исследуемых окон
string gSettingthFileName = ""; // format: # 
string gFolderLogFile = "";
string gFolderOfExecuteFile = "";
// format gSettingthFileName string:
// # comment
// indent = value
// indents:
//	caption_part|caption_part_w - часть строки, которые нужное приложение вставляет в зеголовок, типа: Notepad++, Microsoft Document Explorer, STDU Viewer, Microsoft Visual C++ ит т.п.
//  class_name - имя 
// log_in_wnd_class_name_and_hwnd = yes
// log_in_wnd_class_name_and_hwnd_debug = no
// log_dir = log_dir
// # проверка файлов на изменение.
// watch_dir | watch_dir_w = E:\Projects\Android\ASProjects\test\app\src | *.java;*.xml | маркер_проекта | маркеры_процесса
// # маркер_проекта - для отнесения изменений к тому или иному проекту.
// # маркеры_процесса: част(ь|и) имени пути к файлу процесса, который должен выполняться что-бы начать проверку изменения файлов.
// К примеру: C:\Program Files\Android\Android Studio\bin\studio64.exe;C:\_ProgramF\JetBrains\IntelliJ_IDEA_CE\bin\idea.exe;
// пример:
// watch_dir_w = E:\Projects\Android\ASProjects\test\app\src | *.java | volta | Android Studio\bin\studio;IntelliJ_IDEA_CE\bin\idea.exe
// watch_dir_w = E:\Projects\_Utils\__FileSearche\QFileSearche\src | *.cpp;*.h;*.ui;*.pro | volta | qtcreator.exe;notepad++.exe
// watch_dir_w = E:\Projects\_Utils\WinAPI\winLoger\work | *.cpp;*.h;*.ui;*.pro |volta|notepad++.exe;Microsoft Visual Studio


char gLogFileCaptionString[200] = "";
char gDateTimeUserString[80] = "";
char gDateTimeString[80] = "";
char gDateForLogString[8] = "";
char gModuleFileName[MAX_PATH];
char gCompName[256]; 
char gUserName[50];	


struct winLogItemWnd;
struct winLogItemWnd {
	LONG m_Hwnd; // todo
	string m_caption;
	string m_class;
	struct winLogItemWnd* m_next;
};

struct winLogItem;

struct winLogItem {
	string marker;
	string caption;
	int m_type; // 0-caption, 1-class nname
	bool m_work; // 0-all, 1-work (gLogFileNameWrk)
	struct winLogItemWnd* m_wnd;
	struct winLogItem* Next;
};
struct winLogItem* gRootItem;
struct winLogItem* gLastFoundItem;

/// Задача: Проверка изменений файлов в каталогах
/*
- 
*/
struct folderCheckItem;
struct folderCheckItemFile;
typedef std::list<folderCheckItem*> checkFolderList;
typedef std::list<folderCheckItem*>::iterator checkFolderListIter;
typedef std::list<std::string> stringList;
typedef std::list<std::string>::iterator stringListIter;

//https://en.cppreference.com/w/cpp/container/list

typedef std::list<folderCheckItemFile *> checkFolderFileList;
typedef std::list<folderCheckItemFile*>::iterator checkFolderFileListIter;

struct folderCheckItem {
	string folder;
	string file_extensions;
	string project;
	string proccess_markers;
	bool is_work;
	unsigned long counter; // если сканируем не первый раз и нашли файл новый, это изменение
	checkFolderFileList list;
}; 
struct folderCheckItemFile {
	string filePath;
	//string extension;	
	FILETIME file_tm; // SYSTEMTIME time_lm;
}; //\todo - продолжить тут.

checkFolderList gFolderCheckLst; //folderCheckItem
folderCheckItem* gFolderCheckItemCurent = NULL;
string	   gFolderCurent;
stringList gFolderCICurent_maskList;
stringList gProcessList;

void myBeep() {
	Beep(294, 1000/8);
	Beep(440, 1000/4);
	Beep(262*2, 1000/4);
	Beep(330*2, 1000/4);
	Beep(415, 1000/8);
	Beep(440, 1000);
}


void writeToLog(const char* psInpStr, int vLogType);
void onCalcFolder();


// trdm 2019-09-01 17:20:24
string str_trim(string& str){
	if (!str.empty())	{
		str.erase(str.find_last_not_of(" \n\r\t")+1);
		if (!str.empty())	{
			char ch = str.at(0);
			while ((ch == ' ' || ch == '\t'))	{
				str.erase(0,1);
				ch = str.at(0);
			}
		}
	}
	return str;
}
string str_leftFrom(string& str, string& targ ) {
	string retVal;
	int pos = str.find(targ);
	if (pos)	{
		retVal = str.substr(0, pos);
		str_trim(retVal);
	}
	return retVal;
}
string str_rightFrom(string& str, string& targ ) {
	string retVal;
	int pos = str.find(targ);
	if (pos)	{
		retVal = str.substr(pos+1);
		str_trim(retVal);
	}
	return retVal;
}

stringList str_split(string& psStrIn,string psDelim) {
	stringList list;
	string vTmpStr = psStrIn, strLeft = "", strRight = "";
	int vPos =  vTmpStr.find(psDelim);
	while (vPos > 0)		{
		strLeft = str_leftFrom(vTmpStr, psDelim);
		vTmpStr = str_rightFrom(vTmpStr, psDelim);
		if (strLeft.size() > 0)		{
			list.push_back(strLeft);
		}
		vPos =  vTmpStr.find(psDelim);

	}
	if (vTmpStr.size() > 0)		{
		list.push_back(vTmpStr);
	}
	return list;
}

//http://www.cplusplus.com/reference/locale/tolower/
#include <locale>         // std::locale, std::tolower
#include <boost/algorithm/string.hpp>
// trdm 2021/11/04 12:09
string str_tolower(string& psSrc){
	string vRet = psSrc;
	boost::algorithm::to_lower(vRet); // modifies str
	/*
//	vRet.capacity(psSrc.length());
	std::locale loc;
	for (std::string::size_type i=0; i<psSrc.length(); ++i){
		char cr = psSrc[i];
		vRet.append(cr);
	}
		
//	vRet.append(std::tolower(psSrc[i],loc));
*/
	return vRet;
}

// trdm 2021/11/04 17:18
string str_append_uni(string& psSrc, string psApendix){
	int vLen0 = psSrc.length(), vLenA = psApendix.length();
	string vEnds = psSrc.substr(vLen0-vLenA, vLenA);
	if (vEnds.compare(psApendix)!=0){
		psSrc.append(psApendix);
	}
//	int yy = 20;
	return psSrc;
/*	if (gFolderCurent.at(vLen-1) != '\\')	{
		gFolderCurent.append("\\");
	}
	*/
}



bool settingsStringIsTrue(string& paramText) {
	bool retVal = false;
	if (paramText.compare("yes")==0 || paramText.compare("y")==0 || paramText.compare("1")==0)	{
		retVal = true; // log_in_wnd_class_name_and_hwnd = yes
	}
	return retVal;
}


// © trdm 2019-09-02 10:47:20
void procAnotherSettings(string& paramID, string& paramText) {	
	//string vParamID = paramID.tolo;
	if (paramID.compare("log_dir")==0) {
		string dirStr = paramText; //paramText;
		DWORD attrs = ::GetFileAttributes(dirStr.c_str());	//	if (PathIsDirectoryA(paramText.c_str()))	{
		if (attrs & FILE_ATTRIBUTE_DIRECTORY)	{
			size_t len = dirStr.length();
			if (len > 0) 	{
				gFolderLogFile = dirStr;
				char ch = gFolderLogFile.at(len-1);
				if (ch != '\\')		{
					gFolderLogFile += "\\";
				}
				onCalcFolder();
			}
		}
	} else if (paramID.compare("log_in_wnd_class_name_and_hwnd"))	{
		gLogIn_WndClassAndHwnd = settingsStringIsTrue(paramText); // log_in_wnd_class_name_and_hwnd = yes
	} else if (paramID.compare("log_in_wnd_class_name_and_hwnd_debug"))	{
		gLogIn_WndClassAndHwnd_debug = settingsStringIsTrue(paramText);	
	} else {
		gLogIn_WndClassAndHwnd_debug = settingsStringIsTrue(paramText);	
	}
}

void watch_dir_load_settingth(string& paramID, string& paramText){
	if (paramID.compare("watch_dir")==0 || paramID.compare("watch_dir_w")==0)	{
		// watch_dir = E:\Projects\Android\ASProjects\test\app\src | *.java | маркер_проекта | маркеры_процессов
		string strDelim = "|", strLeft = "", strRight = paramText;
		//gFolderCheckLst; 
		folderCheckItem* item = new folderCheckItem;
		item->counter = 0;
		item->is_work = false;
		if (paramID.compare("watch_dir_w")==0)
			item->is_work = true;
		int vPos =  strRight.find("|"), vPos2 = 0, vCntr = 0, vStrRL = 0;
		while (vPos > 0)		{
			strLeft = str_leftFrom(strRight, strDelim);
			strRight = str_rightFrom(strRight, strDelim);
			vPos2 = strRight.find("|");
			vStrRL = strRight.length();
			if (vCntr == 0)			{
				item->folder = str_trim(strLeft);
			} else if (vCntr == 1)			{
				item->file_extensions = strLeft;
				if (vStrRL > 0 && vPos2 <= 0)	{
					item->project = str_trim(strRight);
				}
			} else if (vCntr == 2)			{
				item->project = str_trim(strLeft);
				if (vStrRL > 0 && vPos2 <= 0)	{
					item->proccess_markers = strRight;
				}
			}
			
			vCntr++;
			vPos =  strRight.find("|");
		}
		gFolderCheckLst.push_back(item);

	}
}

void winLogItemAdd(string& paramID, string& paramText) {
	winLogItem *ItemCur = NULL, *ItemNew = NULL, *ItemLast = NULL;
	bool isWork = false;
	int markerType = -1;
	
	if (paramID.compare("caption_part")==0) {	markerType = 0;		}	
	else if (paramID.compare("caption_part_w")==0)	{	markerType = 0; isWork = true;	}	
	else if (paramID.compare("class_name")==0)		{	markerType = 1; }
	else if (paramID.compare("class_name_w")==0)	{	markerType = 1;	isWork = true;} 
	else if (paramID.compare("watch_dir")==0 || paramID.compare("watch_dir_w")==0)	{
		watch_dir_load_settingth(paramID, paramText); 		return;
	} 
	else 	{		procAnotherSettings(paramID, paramText);		return;
	}
	ItemCur = gRootItem;
	while(ItemCur) {
		if (ItemCur->marker.compare(paramText) == 0){
			ItemNew = ItemCur;
			break;
		}
		ItemLast = ItemCur;
		ItemCur = ItemCur->Next;
	}
	if (!ItemNew){
		ItemCur = new winLogItem;
		if (ItemLast)		{
			ItemLast->Next = ItemCur;
		}
		ItemCur->m_work = isWork;
		ItemCur->marker = paramText;
		ItemCur->m_type = markerType;
		ItemCur->m_wnd = NULL;
		ItemCur->Next = NULL;
	}
	if (!gRootItem)	{
		gRootItem = ItemCur;
	}
}

bool winLogItemTestWC(winLogItem *ItemCur, string& strClass, long lHwnd = 0) {
	bool retVal = false;
	if (!ItemCur){
		return false;
	}
	winLogItemWnd *ItemWndCur = ItemCur->m_wnd, *ItemWndCurFound = NULL;
	if (!ItemWndCur){
		// если первая, значит регистрируем сразу.
		ItemWndCur  = new winLogItemWnd;
		ItemWndCur->m_class = strClass;
		ItemWndCur->m_caption = ItemCur->caption;
		ItemWndCur->m_Hwnd = lHwnd;
		ItemWndCur->m_next = NULL;
		ItemCur->m_wnd = ItemWndCur;
		return true;
	}
	while(ItemWndCur) {
		if ((ItemWndCur->m_Hwnd == lHwnd) && (ItemWndCur->m_class.compare(strClass) == 0)){
			ItemWndCurFound = ItemWndCur;
			if (ItemWndCur->m_caption.compare(ItemCur->caption) != 0)	{
				// если заголовок изменился, тогда регистрируем
				ItemWndCur->m_caption = ItemCur->caption;				
				return true;
			}
			return false;
		}
		if (ItemWndCur->m_next)		{
			ItemWndCur = ItemWndCur->m_next;
		} else {
			break;
		}
	}
	winLogItemWnd* Item  = new winLogItemWnd;
	Item->m_class = strClass;
	Item->m_caption = ItemCur->caption;
	Item->m_Hwnd = lHwnd;
	Item->m_next = NULL;
	ItemWndCur->m_next = Item;
	return true;
}

bool winLogItemTest(string& itemText, string& strClass, long lHwnd = 0) {
	bool retVal = false;
	winLogItem *ItemCur = NULL;
	ItemCur = gRootItem;	//ItemCur = ItemCur->Next;	//std::string::npos pos;
	bool found = false;
	while(ItemCur ) {
		found = false;
		if (ItemCur->m_type == 0)	{
			if (itemText.find(ItemCur->marker) != std::string::npos)		{
				found = true;
			}
		} else if (ItemCur->m_type == 1) {
			if (strClass.compare(ItemCur->marker) == 0)		{
				found = true;
			}
		}
		if (found)	{
			ItemCur->caption = itemText;
			gLastFoundItem = ItemCur;
			gLogFileName = gLogFileNameAll;
			if (ItemCur->m_work) {
				gLogFileName = gLogFileNameWrk;
			}
			try {
				//\todo посмотрим тут ли глюк.
				retVal = winLogItemTestWC(ItemCur, strClass,lHwnd);
			} catch(...) {
				writeToLog("error 20190910115628",3); 
			}
			break;
		}
		/*if (ItemCur->marker.c_str() == itemText.c_str()){			break;		}*/
		ItemCur = ItemCur->Next;
	}
	return retVal;
}

void loadSettings(){
	
	if (gSettingthFileName.empty())	{
		return;
	}
	std::ifstream setFl;	
	setFl.open(gSettingthFileName.c_str(),std::ios_base::in);
	if (!setFl.is_open())	{
		return;
	}
	std::string str, strLeft, strRight, strDelim = "=";
	char line[1000];
	char fistSmb = '0';
	while (setFl.getline(line,1000))	{
		str = line;
		str_trim(str);
		if (!str.empty()){
			fistSmb = str.at(0);
			if (fistSmb == '#' || fistSmb == '/') {}
			else {
				strLeft = str_leftFrom(str, strDelim);
				strRight = str_rightFrom(str, strDelim);
				winLogItemAdd(strLeft, strRight);
			}
		}
		
 	}
	setFl.close();
}

void calcOutputFileName();

// поменял sprintf на sprintf_s
void updateDateTimeUserStrind() {
	SYSTEMTIME st; 				
	GetLocalTime(&st);
	sprintf_s(gLogFileCaptionString,"Data; time; CompName; UserName; ");
	sprintf_s(gDateTimeString,"%04d_%02d_%02d; %02d:%02d:%02d;",st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond); // Дата типа гггг_мм_дд лучше выделяется в N++
	
	sprintf_s(gDateTimeUserString,"%04d_%02d_%02d %02d:%02d:%02d; %s; %s; ",st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, gCompName, gUserName);
	sprintf_s(gDateForLogString,"%04d-%02d",st.wYear, st.wMonth);
}


void writeToLogFileHwnd(const char* psInpStr) {
	if (!gLogIn_WndClassAndHwnd_debug)
		return;
	char outStr[280]; 
	sprintf_s(outStr,"%s %s; \n",gDateTimeString, psInpStr);
	
	if (!gLogFileNameWnd.empty() && psInpStr) {
		std::ofstream vFile;
		vFile.open(gLogFileNameWnd.c_str(), std::ios_base::app);
		vFile << outStr;
		vFile.close();
		gLogFileName = gLogFileNameAll;
	}
}

void writeToLogFile(const char* psInpStr, int vLogType = 0) {
	if (!gLogFileName.empty() && psInpStr) {
		std::ofstream vFile;
		vFile.open(gLogFileName.c_str(), std::ios_base::app);
		vFile << psInpStr;
		vFile.close();
		gLogFileName = gLogFileNameAll;
	}

}

void writeToLog(const char* psInpStr, int vLogType = 0) {
#if _DEBUG
	//return;
#endif
	bool needRecalc = false;
	SYSTEMTIME st; 	GetLocalTime(&st);	
	long dt = st.wYear *10000 + st.wMonth*100 + st.wDay;
	if (gLastDataLogFile != dt) {
		needRecalc = true; 
		gLastDataLogFile = dt;
	}
	updateDateTimeUserStrind(); 
	if (gLogFileName.empty() || needRecalc) {  		
		calcOutputFileName();
	}
	
	if (!gLogFileName.empty() && psInpStr)	{
		char outStr[3000]; 
		sprintf_s(outStr,"%s %s; \n",gDateTimeUserString, psInpStr);
		if (vLogType == 2)	gLogFileName = gLogFileNameWrk;
		if (vLogType == 3) gLogFileName = gLogFileNameWrk;
		writeToLogFile(outStr, vLogType);
		if (vLogType == 3) writeToLogFile(outStr, vLogType);
	}
} 

void calcOutputFileName() {
	gLogFileName = "";
	gLogFileNameWrk = ""; // Лог для работы и калькуляции времени.
	gLogFileNameAll = ""; // Общий лог, все туда

	if (strlen(gDateForLogString) == 0)	{
		updateDateTimeUserStrind();
	}
	
	if (!gFolderLogFile.empty()) {
		gLogFileName = gFolderLogFile;
		gLogFileNameWrk = gFolderLogFile; 
		gLogFileNameAll = gFolderLogFile; 
	}
	if (! gLogFileName.empty()) {
		gLogFileNameWrk += "winLoger";
		gLogFileNameWrk += gDateForLogString;
		gLogFileNameWrk += "_w.log";

		gLogFileNameAll += "winLoger";
		gLogFileNameAll += gDateForLogString;
		gLogFileNameAll += ".log";
		
		gLogFileName = gLogFileNameAll;

		gLogFileNameWnd = gFolderLogFile; 
		gLogFileNameWnd += "winLogerH";
		gLogFileNameWnd += gDateForLogString;
		gLogFileNameWnd += ".log";
		
	}
}



// © trdm 2019-09-02 10:32:44
void onCalcFolder() {
	if (!gFolderOfExecuteFile.empty())	{
		gSettingthFileName = gFolderOfExecuteFile;
		gSettingthFileName += "winLoger.set.txt";
	}
	calcOutputFileName();
}

void calcFolder() {
	std::string logDir = gModuleFileName;
	gFolderOfExecuteFile  = "";
	gFolderLogFile = "";
	int idx, len = logDir.length()-1;
	idx = len;
	if (len) {
		char ch = logDir.at(idx);
		while (idx>=0 && ch != '\\') {
			--idx;
			ch = logDir.at(idx);
		}
		if (idx) {
			gFolderOfExecuteFile = logDir.substr(0, idx+1);
			gFolderLogFile = gFolderOfExecuteFile;
		}
	}
	onCalcFolder();
}

void checkSettings() {
	//\todo - доделать.
	int vLimit = 10;
	if (gScanCounter % vLimit == 0){
		char capt[50];
		sprintf_s(capt,"%s %d","checkSettings: ", gScanCounter);
		//writeToLog(capt);		
	}
	//gScanCounter = 0;
}


bool checkNeedProcc(HWND hw) {
	bool retVal = false;
	//char capt[300]; // "Конфигуратор - < f:\DataBase\SQL_Db\ВольтаОфисР25n\ > - Вольта"

	//int realLenText = GetWindowText(hw, capt, 160); // ограничил с 300 до 200, хватит.

	DWORD pid;
	GetWindowThreadProcessId(hw,&pid);
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

	// Get a handle to the process.
	HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |		PROCESS_VM_READ,		FALSE, pid );

	// Get the process name.
	if (NULL != hProcess )
	{
		HMODULE hMod;
		DWORD cbNeeded;

		if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) )	{
			GetModuleBaseName( hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR) );
		}
	}
	CloseHandle( hProcess );

	// Print the process name and identifier.
	_tcslwr(szProcessName);	 // просто строка для переключения между gLogFileNameWrk и gLogFileNameAll	
	//\todo исключить множественный вывод процесов.
	if (_tcscmp("1cv7s.exe",szProcessName) == 0 || _tcscmp("1cv7l.exe",szProcessName) == 0  || _tcscmp("1cv7.exe",szProcessName) == 0)  {
		retVal = true;
		//_tprintf( TEXT("%s  (PID: %u) (check = %d)\n"), szProcessName, pid, retVal);
	}

	return retVal;
}


BOOL checkStatic(HWND hwnd, LPARAM lp) {
	bool retVal = false;
	HWND vHldWnd = NULL;
	char capt[300];
	char class_name[80];
	int realLenText = 0;
	int lenCapt = 0, lenOther = 0;

	vHldWnd = GetWindow(hwnd,GW_CHILD);
	while (vHldWnd)	{
		GetClassName(vHldWnd,class_name, sizeof(class_name));
		realLenText = GetWindowText(vHldWnd, capt, 300); // ограничил с 300 до 200, хватит.
		if (strcmp(class_name,"Static") == 0){
			if (strstr(capt, "При выполнении реструктуризации") != NULL) {
				retVal = true;				
			} else if ((strstr(capt, "Реорганизация информации закончена!") != NULL))	{
				retVal = true;
			} else if ((strstr(capt, "Ошибка выполнения скрипта!") != NULL))	{
				retVal = true;
			} else if ((strstr(capt, "Ошибка блокировки метаданных.") != NULL))	{
				retVal = true;
			} else if ((strstr(capt, "База данных не может быть открыта в однопользовательском режиме!") != NULL))	{
				retVal = true;
			} else if ((strstr(capt, "Сменилась текущая дата! Изменить рабочую дату?") != NULL))	{
				retVal = true;
			} else if ((strstr(capt, "Сохранение завершено.") != NULL))	{
				retVal = true;
			} else if ((strstr(capt, "Восстановление завершено") != NULL))	{
				retVal = true;
			}

			
			/*База данных не может быть открыта в однопользовательском режиме!*/
			/*Сменилась текущая дата! Изменить рабочую дату?*/
			lenCapt = strlen(capt);
			if (lenCapt > 0){
				//printf("\t\t\t(__)%s\n", capt);
			}
		}
		vHldWnd = GetWindow(vHldWnd, GW_HWNDNEXT);
	}
	return retVal;
}


BOOL checkNeedWindow(HWND hwnd, LPARAM lp){
	BOOL retVal = FALSE;
	char class_name[80];
	char title[300];
	int param = (int) lp;
	GetClassName(hwnd,class_name, sizeof(class_name));
	GetWindowText(hwnd,title,sizeof(title));
	if(strlen(title) > 0){
		//printf("\t\t%s->%s\n", class_name,title);
		if (strcmp(class_name,"#32770") ==  0 )		{
			if (strcmp(title,"Реорганизация информации") == 0)	{
				retVal = TRUE;
				myBeep();
			} else {
				if (checkStatic(hwnd, lp)){
					retVal = TRUE;
					myBeep();
				}
			}
		}
	}
	return retVal;
}

void PrintProcessNameAndID( DWORD processID )
{
	TCHAR szProcessName[MAX_PATH] = TEXT("");
	//!!!!!!!!!!!!! не все процессы сканируются.. :(
	// https://www.cyberforum.ru/win-api/thread448472.html
	// Get a handle to the process.
	HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |	PROCESS_VM_READ, FALSE, processID );
	DWORD vdv1, vSizePth = MAX_PATH;
    char vPath[MAX_PATH];
	// Get the process name.
	if (NULL != hProcess ) 	{
		HMODULE hMod;
		DWORD cbNeeded;
		MyQueryFullProcessImageNameA(hProcess, 0x00000001, vPath, &vSizePth );

		if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) )	{
			vdv1 = GetModuleFileNameEx( hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR) );
		}
	} else {

	}

	// Print the process name and identifier.
	//GetVolumePathName(vPath,vPath2,MAX_PATH);
	string vProcName = szProcessName;
	if (vSizePth > 0 && vProcName.size() == 0)	{
		vProcName = vPath;
	}
	int vPos = vProcName.find(":\\"); //+		vProcName	"\Device\HarddiskVolume2\Windows\System32\psxss.exe"	std::basic_string<char,std::char_traits<char>,std::allocator<char> >
	int vPos2 = vProcName.find("\\Device");
	if (vPos >= 0 || vPos2 >= 0)	{
		bool vAExist = false;
		string vPs = "";
		stringListIter iter = gProcessList.begin();
		while (iter != gProcessList.end())		{
			vPs = *iter;
			if (vProcName.compare(vPs) == 0)			{
				vAExist = true;
				break;
			}
			iter++;
		}
		if (!vAExist)		{
			gProcessList.push_back(vProcName);

		}
	}
	//_tprintf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );

	CloseHandle( hProcess );
}

void scanAllProcesses() {
	///// ms-help://MS.VSCC.v80/MS.MSDN.v80/MS.WIN32COM.v10.en/perfmon/base/enumerating_all_processes.htm
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;
	if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
		return;
	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);
	// Print the name and process identifier for each process.
	for ( i = 0; i < cProcesses; i++ )		PrintProcessNameAndID( aProcesses[i] );
	//gProcessList.sort();
	//gProcessList.unique();
}

void recursuive_file_search_about_erase(string psFName){
	string to_log_str = gFolderCheckItemCurent->project;
	to_log_str.append("; del file: ");
	to_log_str.append(psFName);
	int vNLog = gFolderCheckItemCurent->is_work ? 2 : 0;
	writeToLog(to_log_str.c_str(), vNLog);

}

// trdm 2021/11/04 12:05 нашли файло...
void recursuive_file_search_registred_found(WIN32_FIND_DATA* psFindFileData, const char* dir){
	if (gFolderCurent.at(gFolderCurent.length()-1) != '\\')	{
		int uu = 200;
		gFolderCurent.append("\\");
	}
	string vFName = psFindFileData->cFileName;
	string vFFName = dir;	vFFName.append(vFName);
	bool vFound = false, vErase = false;
	folderCheckItemFile* vItemFile = NULL;
	//checkFolderFileList &list = *(gFolderCheckItemCurent->list);
	
	checkFolderFileListIter itFile = gFolderCheckItemCurent->list.begin();
	string to_log_str, to_log_oper;
	while (itFile != gFolderCheckItemCurent->list.end()){
		vItemFile = *itFile;
		if (vItemFile->filePath == vFFName)	{
			vFound = true;
			break;
		} else {			
		}
		itFile++;
	}
	if (!vFound) {
		vItemFile = new folderCheckItemFile;
		vItemFile->filePath = vFFName;
		vItemFile->file_tm = psFindFileData->ftLastWriteTime;
		gFolderCheckItemCurent->list.push_back(vItemFile);
		if (gFolderCheckItemCurent->counter > 0){
			to_log_oper = " new file: ";
		}
	} else {
		// Уже есть файл.
		FILETIME ft = vItemFile->file_tm, ft2=psFindFileData->ftLastWriteTime;
		if (!(ft.dwHighDateTime == ft2.dwHighDateTime && ft.dwLowDateTime == ft2.dwLowDateTime))	{
			to_log_oper = " upd file: ";
		}

		vItemFile->file_tm = psFindFileData->ftLastWriteTime;
	}
	if (to_log_oper.length() > 0)	{
		to_log_str = gFolderCheckItemCurent->project;
		to_log_str.append("; ");
		to_log_str.append(to_log_oper);
		to_log_str.append(vFFName);
		int vNLog = gFolderCheckItemCurent->is_work ? 2 : 0;
		writeToLog(to_log_str.c_str(),vNLog);
	}

}


void recursuive_file_search_check_file_name(WIN32_FIND_DATA* psFindFileData, const char* dir){
	string vFName = psFindFileData->cFileName;
	vFName = str_tolower(vFName);
	//vFName = vFName.toLower
	string vMask, vExt = "";
	bool vStMask = false; /// стандартная маска типа ""*.java"
	if (gFolderCICurent_maskList.size() > 0)	{
		bool vFound = false;
		int vPos = 0, vPos2 = 0;
		stringListIter iter = gFolderCICurent_maskList.begin();
		while (iter != gFolderCICurent_maskList.end())	{
			vMask = *iter;
			//vMask = vMask.
			if (vMask.compare("*.*")==0 || vMask.compare(".*")==0)	{
				vFound = true;
				break;

			} else {
				if (vMask.at(0) == '*'){
					vMask = vMask.substr(1,vMask.length()-1);
				}
				vPos = vFName.rfind(vMask);
				if (vPos != -1)	{
					vPos2 = vPos+vMask.length();
					if (vFName.length() == (vPos2))	{
						vFound = true;
						break;
					}
				}

			}
			iter++;
		}
		if (vFound){
			recursuive_file_search_registred_found(psFindFileData, dir);
		}
	}
}



// trdm 2021/11/04 10:34
// https://www.cyberforum.ru/cpp-beginners/thread1294184.html
void recursuive_file_search(const char* dir)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	char buff[MAX_PATH] = {0};
	
	char dir_find[MAX_PATH] = {0};
	strcpy(dir_find, dir);
	strcat(dir_find, "*");

	hFind = FindFirstFile(dir_find, &FindFileData);
	bool vIsDir = false, vIsDot = false, vIsHiden;


	if (hFind == INVALID_HANDLE_VALUE)	{
		//printf ("Invalid file handle. Error is %u.\n", GetLastError());

	} 	else 	{
		vIsDir = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		vIsHiden = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);		
		vIsDot = (strcmp(FindFileData.cFileName, ".")==0 || strcmp(FindFileData.cFileName, "..")==0);
		if(vIsDir && !vIsDot && !vIsHiden)
		{

			strcpy(buff, dir);
			strcat(buff, FindFileData.cFileName);
			strcat(buff, "\\");
			gFolderCurent = buff;
			recursuive_file_search(buff);

		}
		///////////////////////////////////////////////////////
		//////////////////////////////////////////////////////
		//вот здесь поиск файлов по маске
		if (!vIsDir && !vIsDot)	{
			recursuive_file_search_check_file_name(&FindFileData, dir);
		}
		// List all the other files in the directory.
		while (FindNextFile(hFind, &FindFileData) != 0)
		{
			vIsDir = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); // ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
			vIsHiden = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);		
			vIsDot = (strcmp(FindFileData.cFileName, ".")==0 || strcmp(FindFileData.cFileName, "..")==0);
			if(vIsDir && !vIsDot && !vIsHiden)
			{
				strcpy(buff, dir);
				strcat(buff, FindFileData.cFileName);
				strcat(buff, "\\");
				gFolderCurent = buff;
				recursuive_file_search(buff);
			}
			//////////////////////////////////////////////////////
			//вот здесь поиск файлов по маске
			if (!vIsDir && !vIsDot)			{
				recursuive_file_search_check_file_name(&FindFileData, dir);
			}
		}
		FindClose(hFind);
	}
}

// trdm 2021/11/03 21:20
void checkFileSystemPath() {
	str_append_uni(gFolderCheckItemCurent->folder,"\\");
	string vFolder = gFolderCheckItemCurent->folder;
	gFolderCurent = vFolder;
	string vFolderCurent = gFolderCurent;
	gFolderCICurent_maskList = str_split(gFolderCheckItemCurent->file_extensions,";");
	recursuive_file_search(vFolder.c_str());

	gFolderCheckItemCurent->counter++;
	folderCheckItemFile* vItemFile;
	checkFolderFileListIter itFile = gFolderCheckItemCurent->list.begin();
	string vFFName, to_log_oper;
	bool vExist = false;
	while (itFile != gFolderCheckItemCurent->list.end()){
		vItemFile = *itFile;
		vFFName = vItemFile->filePath;
		vExist = (::GetFileAttributes(vFFName.c_str()) != DWORD(-1));
		if (!vExist)	{			
			to_log_oper = vItemFile->filePath;
			recursuive_file_search_about_erase(to_log_oper);
			itFile = gFolderCheckItemCurent->list.erase(itFile);
			continue;
		}
		itFile++;
	}

}

void checkFileSystem() {
	gFsBalance = gFsBalance - gTimerInterval;
	if (gFsBalance > 0){
		return;
	}
	gFsBalance = gFsCheckInterval; // balance - остаток средств
	
	// проверяем....
	if (gFolderCheckLst.size() == 0)	{
		return;
	}

	scanAllProcesses();
	stringListIter vSlIter = gProcessList.begin();

	bool vNeedScanFS = true;
	int vPos = 0;
	checkFolderListIter iter = gFolderCheckLst.begin();
	string vPm = "", vPPath="";
	gFolderCheckItemCurent = NULL;
	while (iter != gFolderCheckLst.end())	{
		vNeedScanFS = true;
		gFolderCheckItemCurent = *iter;
		stringList vList2 = str_split(gFolderCheckItemCurent->proccess_markers,";");
		if (vList2.size() > 0)	{
			vNeedScanFS = false;
			stringListIter vList2Iter = vList2.begin();
			while (vList2Iter != vList2.end())	{
				vPm = *vList2Iter;
				vSlIter = gProcessList.begin();
				while (vSlIter != gProcessList.end()){
					vPPath = *vSlIter;
					vPos = vPPath.find(vPm);
					if (vPos >= 0){
						vNeedScanFS = true;
						break;
					}
					vSlIter++;
				}
				if (vNeedScanFS)	{
					break;
				}
				vList2Iter++;
			}
		}
		if (vNeedScanFS){
			checkFileSystemPath();			
		}
		iter++;
	}

}


void mainWork() {
	HANDLE mutLoc;
#if _DEBUG
	mutLoc = CreateMutex(NULL, FALSE, "winLoger.mainWork-Dev");
#else
	mutLoc = CreateMutex(NULL, FALSE, "winLoger.mainWork");
#endif
	if (mutLoc == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		return; 
	}
	//trdm 2020-07-02
	UINT vSek = 10; // 30
	DWORD vTc = GetTickCount() / 1000;
	bool vNeedCheckWnd = false;
	if (gLastRunWindowsCheck == 0)	{
		vNeedCheckWnd = true;
	} else {		
		if (vTc - gLastRunWindowsCheck >= vSek) {
			vNeedCheckWnd = true;			
		}
	}
	if (vNeedCheckWnd ) gLastRunWindowsCheck = vTc;


	gScanCounter += 1;
	checkSettings();

	HWND gDesctWnd = GetDesktopWindow();
	HWND vHldWnd = NULL;
	vHldWnd = GetWindow(gDesctWnd,GW_CHILD);
	// В заголовки размером в 300 символов не помещается 
	char capt[300], capt3[1000];
	char capt2[1000];
	char class_name[100];
	int realLenText = 0;
	int lenCapt = 0, lenOther = 0;
	string capt_s = "";
	string class_s = "";
	int vLocCounter = 0;
	writeToLogFileHwnd("--------------------------------");
	long lHwnd = 0;
	while (vHldWnd)	{
		// GetWindowThreadProcessId
		realLenText = GetWindowText(vHldWnd, capt, 160); // ограничил с 300 до 200, хватит.
		lenCapt = strlen(capt);
		if (lenCapt > 10)		{
			vLocCounter += 1;
			if (lenCapt > 210) {
				int stoper = 100;
			}
			capt_s = capt;
			GetClassName(vHldWnd, class_name, 100);
			class_s = class_name;
			lenOther = lenCapt + 4 + strlen(class_name) + sizeof(long);
			if (gLogIn_WndClassAndHwnd_debug)	{				
				sprintf_s(capt3,"%d; %s; %s",lHwnd,class_name, capt);			
				writeToLogFileHwnd(capt3); 			
			}
			lHwnd = (long)vHldWnd;
			bool stat = false;
			try {
				stat = winLogItemTest(capt_s,class_s, lHwnd);
			} catch(...) {
				writeToLog("error 20190910115728",3); 
			}

			if (stat) 	{
				if (gLastFoundItem) {
					if (gLogIn_WndClassAndHwnd)	{
						lenOther = lenCapt + 12 + strlen(class_name) + sizeof(long);
						sprintf_s(capt2,"(%s (%s | %d)) -> %s",gLastFoundItem->marker.c_str(),class_name, lHwnd, capt);
					} else {
						sprintf_s(capt2,"(%s) -> %s",gLastFoundItem->marker.c_str(), capt);
					}
					writeToLog(capt2); // тут вылет

					if (gLastFoundItem->m_work)	{
						gLogFileName = gLogFileNameAll;
						writeToLog(capt2);
					}
				} else {
					//Галактека опасносте!!!
					writeToLog("error #20190910112916",2); 
				}
			} 
		}
		// trdm 2020-07-02 16:09:16 
		if (vNeedCheckWnd) {
			if ( checkNeedProcc(vHldWnd)) {
				checkNeedWindow(vHldWnd,0);
			}

		}
		vHldWnd = GetWindow(vHldWnd, GW_HWNDNEXT);

	}
	if (gFolderCheckLst.size() > 0)	{
		checkFileSystem();
	}
	CloseHandle(mutLoc);
}

void init() {
	unsigned long size = 256;   
	GetComputerName( gCompName, &size );
#if _DEBUG
	//gLogIn_WndClassAndHwnd_debug =true;
#endif
	gLogIn_WndClassAndHwnd_debug =false;
	DWORD len=51;				
	GetUserName(gUserName,&len);

	gRootItem = NULL;
	gLastFoundItem = NULL;
	calcFolder();
	calcOutputFileName();
	loadSettings();
	gLogFileName = gLogFileNameWrk;
#if _DEBUG
	char* startStr = "Старт winLogerD.exe";
#else
	char* startStr = "Старт winLoger.exe";
#endif
	string vStartStr = startStr;
	//vStartStr.append(50);
	writeToLog(startStr ,2); // просто логирование старта в gLogFileNameWrk
	writeToLog(startStr); // а поттом в all

}

BOOL SetPrivilege( HANDLE hToken,          // access token handle
				  LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
				  BOOL bEnablePrivilege   // to enable or disable privilege
				  ) 
{
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if ( !LookupPrivilegeValue( 
		NULL,            // lookup privilege on local system
		lpszPrivilege,   // privilege to lookup 
		&luid ) )        // receives LUID of privilege
	{
		printf("LookupPrivilegeValue error: %u\n", GetLastError() ); 
		return FALSE; 
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	// Enable the privilege or disable all privileges.

	if ( !AdjustTokenPrivileges(		hToken, 		FALSE, 		&tp, 		sizeof(TOKEN_PRIVILEGES), 
		(PTOKEN_PRIVILEGES) NULL, 		(PDWORD) NULL) )	{ 
			printf("AdjustTokenPrivileges error: %u\n", GetLastError() ); 
			return FALSE; 
	} 

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)	{
		printf("The token does not have the specified privilege. \n");
		return FALSE;
	} 

	return TRUE;
}


/// Получаем права отладчика https://www.cyberforum.ru/win-api/thread448472.html
BOOL EnableDebugPrivilages() {
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,&hToken );
	return SetPrivilege(hToken, SE_DEBUG_NAME, TRUE);
}


VOID CALLBACK myTimerProc( HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime){	mainWork(); }

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	HANDLE mut;
#if _DEBUG
	mut = CreateMutex(NULL, FALSE, "winLoger.exe-trdm-Dev0");
#else
	mut = CreateMutex(NULL, FALSE, "winLoger.exe-trdm");
#endif
	if (mut == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		return -1; 
	}
	//gDebug = true;
	if (strstr(lpCmdLine," -d") != NULL) {
		gDebug = true;
	}
	// проблема003:
	HMODULE hMod = LoadLibrary("kernel32.dll");
	MyQueryFullProcessImageNameA = (_QueryFullProcessImageName)GetProcAddress(hMod, "QueryFullProcessImageNameA");


	EnableDebugPrivilages();
	GetModuleFileName(hInstance,gModuleFileName,MAX_PATH);
	init();
	mainWork();
	//return -1; 

	gTimerId = SetTimer(NULL, 0, gTimerInterval*1000, &myTimerProc); // 10 секунд
	
	
	MSG msg;                                    //сруктура для очереди сообщений
	//стандартная очередь сообщений
	while(GetMessage(&msg,NULL,0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

	CloseHandle(mut);
	KillTimer(NULL, gTimerId);
	return 0;
}



