using namespace std;

// ��������003: ������� QueryFullProcessImageName ��� � ��������������.
//https://www.cyberforum.ru/win-api/thread2110349.html
typedef BOOL (WINAPI* _QueryFullProcessImageName)(HANDLE, DWORD, LPTSTR, PDWORD);
_QueryFullProcessImageName MyQueryFullProcessImageNameA;
