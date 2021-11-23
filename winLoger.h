using namespace std;

// проблема003: функции QueryFullProcessImageName нет в заголовочниках.
//https://www.cyberforum.ru/win-api/thread2110349.html
typedef BOOL (WINAPI* _QueryFullProcessImageName)(HANDLE, DWORD, LPTSTR, PDWORD);
_QueryFullProcessImageName MyQueryFullProcessImageNameA;
