#include <windows.h>
int main() {
 HKEY key=nullptr;
 const LSTATUS result=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software",0,KEY_QUERY_VALUE,&key);
 if(result==ERROR_SUCCESS) RegCloseKey(key);
 return result==ERROR_SUCCESS?0:1;
}
