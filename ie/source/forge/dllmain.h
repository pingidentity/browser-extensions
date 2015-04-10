#include <ScriptExtensions.h>
#include "vendor.h"


#ifdef STACKWALKER
#include "..\SW.h" // StackWalker

class MyStackWalker : public StackWalker
{
private:
    int StringToWString(std::wstring &ws, const std::string &s)
    {
        std::wstring wsTmp(s.begin(), s.end());
        ws = wsTmp;
        return 0;
    }
public:
  MyStackWalker() : StackWalker() {}
  MyStackWalker(DWORD dwProcessId, HANDLE hProcess) : StackWalker(dwProcessId, hProcess) {}
  virtual void OnSymInit(LPCSTR szSearchPath, DWORD symOptions, LPCSTR szUserName) {}
  virtual void OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion) {}
/*
  virtual void OnDbgHelpErr(LPCSTR szText, DWORD gle, DWORD64 addr) {
      wstring wText; 
      StringToWString(wText, szText);
      wText.erase(wText.find_last_not_of(L" \n\r\t")+1); 
      logger->debug(L"[SW::OnDbgHelpErr] "+wText +  L", error=" + boost::lexical_cast<wstring>(gle) + L", address=" + boost::lexical_cast<wstring>(addr));
  }
*/
  virtual void OnOutput(LPCSTR szText) { 
      wstring wText; 
      StringToWString(wText, szText);
      wText.erase(wText.find_last_not_of(L" \n\r\t")+1); 
      if (wText.find(L"c:\\dev") != wstring::npos)
      {
          // log only our code
          logger->debug(L"    >>> "+wText);
      }
  }
};
#endif // STACKWALKER

class CForgeModule : public ATL::CAtlDllModuleT< CForgeModule >
{
public :
    DECLARE_LIBID(LIBID_ForgeLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_FORGE, VENDOR_UUID_NSTR(VENDOR_UUID_ForgeAppId))

    // We supply additional replacements for registry scripts so we don't have
    // to hardcode GUIDs into the *.rgs files.
    HRESULT AddCommonRGSReplacements(IRegistrarBase *pRegistrar) {
        HRESULT hr = __super::AddCommonRGSReplacements(pRegistrar);
        if (FAILED(hr))
            return hr;

        const struct _map {
            wchar_t *key;
            GUID guid;
        } map[] = {
            { L"LIBID_ForgeLib", LIBID_ForgeLib },
            { L"CLSID_NativeBackground", CLSID_NativeBackground },
            { L"CLSID_NativeControls", CLSID_NativeControls },
            { L"CLSID_NativeExtensions", CLSID_NativeExtensions },
            { L"CLSID_NativeMessaging", CLSID_NativeMessaging },
            { 0, 0 }
        };

        for (const struct _map *m = map; m->key; m++) {
            hr = pRegistrar->AddReplacement(m->key, CComBSTR(m->guid));
            if (FAILED(hr))
                return hr;
        }
        return S_OK;
    }

    bfs::wpath callerPath;
    bfs::wpath moduleExec;
    bfs::wpath modulePath;
    wstring    moduleCLSID;
    GUID       moduleGUID;
    HMODULE    moduleHandle;
    Manifest::pointer moduleManifest;

    virtual LONG Lock() throw()
    {
        LONG lCnt = CAtlDllModuleT::Lock();
        logger->debug(L"CForgeModule::Lock="  + boost::lexical_cast<wstring>(lCnt));
#ifdef STACKWALKER
//        MyStackWalker sw; sw.ShowCallstack(); 
#endif // STACKWALKER
        return lCnt;
    };

    virtual LONG Unlock() throw()
    {
        LONG lCnt = CAtlDllModuleT::Unlock();
        logger->debug(L"CForgeModule::Unlock="  + boost::lexical_cast<wstring>(lCnt));
#ifdef STACKWALKER
//        MyStackWalker sw; sw.ShowCallstack(); 
#endif // STACKWALKER
        return lCnt;
    };

    virtual LONG GetLockCount( ) throw() 
    {
        LONG lCnt = CAtlDllModuleT::GetLockCount();
        logger->debug(L"CForgeModule::GetLockCount="  + boost::lexical_cast<wstring>(lCnt));
#ifdef STACKWALKER
//        MyStackWalker sw; sw.ShowCallstack(); 
#endif // STACKWALKER
        return lCnt;
    };
};

extern class CForgeModule _AtlModule;
