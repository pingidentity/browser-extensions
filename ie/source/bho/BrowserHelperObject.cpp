#include "stdafx.h"
#include "BrowserHelperObject.h"
#include "HTMLDocument.h"
#include "AccessibleBrowser.h"
#include "dllmain.h"
#include "vendor.h"
#include <util.h>
#include <WindowsMessage.h>
#include <proxy/Commands.h>
#include <Windows.h>

/**
 * Static initialization 
 */
_ATL_FUNC_INFO CBrowserHelperObject::OnNavigateComplete2Info = {
    CC_STDCALL, VT_EMPTY, 2, {
        VT_DISPATCH,
        VT_VARIANT | VT_BYREF
    }
};
_ATL_FUNC_INFO CBrowserHelperObject::OnDocumentCompleteInfo = {
    CC_STDCALL, VT_EMPTY, 2, {
        VT_DISPATCH,
        VT_VARIANT | VT_BYREF
    }
};
_ATL_FUNC_INFO CBrowserHelperObject::OnWindowStateChangedInfo = {
    CC_STDCALL, VT_EMPTY, 2, {
        VT_UI4,
        VT_UI4
    }
};
_ATL_FUNC_INFO CBrowserHelperObject::OnDownloadCompleteInfo = {
    CC_STDCALL, VT_EMPTY, 0, {}
};
_ATL_FUNC_INFO CBrowserHelperObject::OnFocusInInfo = {
    CC_STDCALL, VT_EMPTY, 1, {
        VT_DISPATCH,
    }
};

static const wstring ACCESSIBLE_PROPERTY(L"accessible");
static const wstring EXTENSIONS_PROPERTY(L"extensions");
static const wstring MESSAGING_PROPERTY(L"messaging");
static const wstring FORGE_SCRIPT_INJECTION_TAG(L"forgeScriptInjectionTag");

/**
 * Construction
 */
CBrowserHelperObject::CBrowserHelperObject()
    : m_isConnected(false),
      m_nativeAccessible(),
      m_frameProxy(NULL)
{
    logger->debug(L"----------------------------------------------------------");
    logger->debug(L"BrowserHelperObject Initializing...");

    logger->debug(L"BrowserHelperObject::BrowserHelperObject"
                  L" -> " + _AtlModule.moduleManifest->uuid +
                  L" -> " + _AtlModule.moduleCLSID);

    // load script extensions - TODO user level error handling for load failure
    m_scriptExtensions = 
        ScriptExtensions::pointer(new ScriptExtensions(_AtlModule.modulePath));
    if (!m_scriptExtensions->manifest) {
        logger->error(L"Failed to read manifest file: " +
                      m_scriptExtensions->pathManifest.wstring() + 
                      L" at: " + _AtlModule.modulePath.wstring());
        ::MessageBox(NULL,
                     wstring(L"Failed to read manifest. Please check that "
                             L" manifest.json file is present at " +
                             _AtlModule.modulePath.wstring() +
                             L" and properly configured.").c_str(),
                     VENDOR_COMPANY_NAME,
                     MB_TASKMODAL | MB_ICONEXCLAMATION);
        // TODO halt BHO init and exit gracefully
        return;
    }

    LogIESetting(TEXT("Start Page"));
    LogIESetting(TEXT("Enable Browser Extensions"));
    LogSecuritySites();
}

void LogIESetting(LPCTSTR hValueName) {
	HKEY hKey;
	wchar_t lszValue[1024];
	DWORD dwType = REG_SZ;
	DWORD dwSize = 1024;
	long returnStatus = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Microsoft\\Internet Explorer\\Main"), 0, KEY_QUERY_VALUE, &hKey);
	if (returnStatus == ERROR_SUCCESS)
	{
		returnStatus = RegQueryValueEx(hKey, hValueName, NULL, &dwType, (LPBYTE)&lszValue, &dwSize);
		RegCloseKey(hKey);
		if (returnStatus == ERROR_SUCCESS)
		{
			logSystem(hValueName + L" = " + lszValue);
		}
		else {
		    logSystem(L"Can't read" + hValueName);
		}
	}
}

void LogSecuritySites()
{
    HKEY hKey = { 0 };
	LPCTSTR path = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\ZoneMap\\Domains");
	if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_ENUMERATE_SUB_KEYS, &hKey) != ERROR_SUCCESS)
		return;

	LogAllEnums(hKey);
}

void LogAllEnums(HKEY hKey)
{
	DWORD index = 0;           // enumeration index
	TCHAR keyName[256] = { 0 };  // buffer to store enumerated subkey name
	DWORD keyLen = 256;        // buffer length / number of TCHARs copied to keyName

	while (RegEnumKeyEx(hKey, index++, keyName, &keyLen, 0, 0, 0, 0) == ERROR_SUCCESS) {
	    logger->logSystem(L"Domain: " + keyName);
		keyLen = 256;
		HKEY hSubKey = { 0 };
		if (RegOpenKeyEx(hKey, keyName, 0, KEY_ALL_ACCESS, &hSubKey) == ERROR_SUCCESS) {
			DWORD dwType = REG_DWORD;
			DWORD dwReturn;
			DWORD dwSize = sizeof(DWORD);
			long returnStatus = RegQueryValueExW(hSubKey, TEXT("https"), NULL, &dwType, reinterpret_cast<LPBYTE>(&dwReturn), &dwSize);
			if (returnStatus == ERROR_SUCCESS)
			{
				if (dwReturn == 2) {
					logger->logSystem("---> is trusted");
				}
				else if (dwReturn == 4) {
					logger->logSystem("---> is restricted");
				}
			}
			else if (returnStatus == ERROR_FILE_NOT_FOUND) {
				logAllEnums(hSubKey);
			}

			RegCloseKey(hSubKey);
		}
	}
}

CBrowserHelperObject::~CBrowserHelperObject()
{
/*
    if (m_frameProxy)
    {
        // it seems the m_frameProxy might need to be deleted but don't do it, it causes double free/memory corruption as it is likely done from somewhere else
        logger->debug(L"CBrowserHelperObject::~CBrowserHelperObject unFIXED LEAK (" + boost::lexical_cast<wstring>(sizeof(*m_frameProxy)) + L" bytes)");
        delete m_frameProxy;
        m_frameProxy = NULL;
    }
*/
}

/**
 * Interface: IObjectWithSite::SetSite
 */
STDMETHODIMP CBrowserHelperObject::SetSite(IUnknown *unknown)
{
    HRESULT hr;
    if (unknown == NULL) {
        hr = this->OnDisconnect(unknown);
    } else {
        hr = this->OnConnect(unknown);
    }
    if (FAILED(hr)) {
        logger->error(L"BrowserHelperObject::SetSite failed"
                      L" -> " + logger->parse(hr));
        return hr;
    }
    return IObjectWithSiteImpl<CBrowserHelperObject>::SetSite(unknown);
}


/**
 * Event: OnConnect
 */
HRESULT CBrowserHelperObject::OnConnect(IUnknown *unknown) 
{
    HRESULT hr;

    logger->debug(L"BrowserHelperObject::OnConnect" 
                  L" -> " + _AtlModule.moduleManifest->uuid);

    if (m_isConnected) {
        logger->debug(L"  -> OK already connected");
        return S_OK;
    }

    // helpful values
    wstring uuid = _AtlModule.moduleManifest->uuid;
    wstring path = (_AtlModule.modulePath / 
                    m_scriptExtensions->manifest->background_page).wstring();

    // browsers and interfaces and sinks oh my
    CComQIPtr<IServiceProvider> sp(unknown);
    hr = sp->QueryService(IID_IWebBrowserApp, IID_IWebBrowser2, 
                          (void**)&m_webBrowser2);
    if (FAILED(hr)) {
        logger->debug(L"CBrowserHelperObject::SetSite "
                      L"failed to get IWebBrowser2"
                      L" -> " + logger->parse(hr));
        return hr;
    }
    hr = WebBrowserEvents2::DispEventAdvise(m_webBrowser2);
    if (FAILED(hr)) {
        logger->error(L"BrowserHelperObject::OnConnect "
                      L"failed to sink DWebBrowserEvents2"
                      L" -> " + logger->parse(hr));
        return hr;
    } 
    m_isConnected = true; // TODO deprecate

    // do we need to support a toolbar button for this extension?
    CComPtr<INativeControls> nativeControls;
    wstring    default_title;
    bfs::wpath default_icon;
    default_title = m_scriptExtensions->manifest->browser_action.default_title;
    default_icon  = (_AtlModule.modulePath / 
                     m_scriptExtensions->manifest->browser_action.default_icon).wstring();
    if (m_scriptExtensions->manifest->browser_action.default_icon == L"" ||
        !bfs::exists(default_icon)) {
        logger->info(L"CBrowserHelperObject::OnConnect skipping forge.button.* support");
        goto nativebackground; // return S_OK;
    }

    // get browser HWND
    SHANDLE_PTR phwnd;
    hr = m_webBrowser2->get_HWND(&phwnd);
    if (FAILED(hr)) {
        logger->error(L"CBrowserHelperObject::OnConnect failed to get browser hwnd");
        return hr;
    }
    HWND hwnd = reinterpret_cast<HWND>(phwnd);

    HWND toolbar, target;
    if (!WindowsMessage::GetToolbar(hwnd, &toolbar, &target)) {
        logger->error(L"CBrowserHelperObject::OnConnect failed to get toolbar hwnd");
        goto nativebackground; // TODO this is the place to do any haxoring if we decided to force.enable the CommandBar
    }

    // inject proxy lib into frame process 
    if (!m_frameProxy) {
        m_frameProxy = new FrameProxy(uuid, 
                                      _AtlModule.moduleHandle, 
                                      toolbar, 
                                      target,
                                      default_title, 
                                      default_icon.wstring());
        if (!m_frameProxy->IsOnline()) {
            delete m_frameProxy; // LEAKFIX
            m_frameProxy = NULL;
            logger->error(L"CBrowserHelperObject::OnConnect failed to connect to FrameProxy");
            goto nativebackground;
        }
    }

    // inform NativeControls of browser's HWND - TODO s/load/advise
    logger->debug(L"CBrowserHelperObject::OnConnect getting NativeControls");
    hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); 
    hr = ::CoCreateInstance(CLSID_NativeControls, 
                            NULL,
                            CLSCTX_LOCAL_SERVER, 
                            IID_INativeControls,
                            (LPVOID*)&nativeControls);
    if (FAILED(hr)) {
        logger->error(L"CBrowserHelperObject::OnConnect "
                      L"failed to obtain NativeControls"
                      L" -> " + logger->parse(hr));
        ::CoUninitialize();
        return hr;
    }
    hr = nativeControls->load(CComBSTR(uuid.c_str()), 
                              CComBSTR(_AtlModule.modulePath.wstring().c_str()),
                              m_instanceId, (ULONG)hwnd);
    if (FAILED(hr)) {
        logger->error(L"CBrowserHelperObject::OnConnect "
                      L"failed to inform NativeControls"
                      L" -> " + logger->parse(hr));
        ::CoUninitialize();
        return hr;
    }
    ::CoUninitialize();

 nativebackground:
    // attach background page
    hr = Attach::NativeBackground(uuid, path,
                                  _AtlModule.moduleManifest->logging.console,
                                  &m_nativeBackground.p, // "T** operator&() throw()" asserts on p==NULL
                                  &m_instanceId); 
    if (FAILED(hr)) {
        logger->error(L"BrowserHelperObject::OnConnect "
                      L"failed to attach background page"
                      L" -> " + logger->parse(hr));
        return hr;
    }         

    logger->debug(L"BrowserHelperObject::OnConnect done" 
                  L" -> " + _AtlModule.moduleManifest->uuid +
                  L" -> " + boost::lexical_cast<wstring>(m_instanceId));
    return S_OK;
}

HRESULT CBrowserHelperObject::DetachFromDocEvents()
{
    HRESULT hr;

    if (this->m_currentDocument) {
        hr = HtmlDocEvents2::DispEventUnadvise(this->m_currentDocument);
        if (hr == CONNECT_E_NOCONNECTION || SUCCEEDED(hr) ) {
            logger->debug(L"BrowserHelperObject::DetachFromDocEvents unsunk events");
        }else{
            logger->error(L"BrowserHelperObject::DetachFromDocEvents failed to unsink events");
            return hr;
        }
        this->m_currentDocument = NULL;
    }

    return S_OK;
}

/**
 * Event: OnDisconnect
 */
HRESULT CBrowserHelperObject::OnDisconnect(IUnknown *unknown) 
{
    logger->debug(L"BrowserHelperObject::OnDisconnect()"
                  L" -> " + boost::lexical_cast<wstring>(m_instanceId) +
                  L" -> " + _AtlModule.moduleManifest->uuid);
    
    HRESULT hr;

    // NativeExtensions
    //logger->debug(L"BrowserHelperObject::OnDisconnect() release NativeExtensions");

    // NativeMessaging
    logger->debug(L"BrowserHelperObject::OnDisconnect() release NativeMessaging");
    if (m_nativeMessaging) {
        m_nativeMessaging->unload(CComBSTR(_AtlModule.moduleManifest->uuid.c_str()), 
                                  m_instanceId);
    }

    // NativeAccessible
    logger->debug(L"BrowserHelperObject::OnDisconnect() release NativeAccessible");
    if (m_nativeAccessible) m_nativeAccessible.reset();

    // NativeBackground
    if (m_nativeBackground) {
        logger->debug(L"BrowserHelperObject::OnDisconnect() release NativeBackground"); 
        hr = m_nativeBackground->unload(CComBSTR(_AtlModule.moduleManifest->uuid.c_str()), 
                                        m_instanceId);
        if (FAILED(hr)) {
            logger->warn(L"BrowserHelperObject::OnDisconnect "
                         L"failed to unload NativeBackground"
                         L" -> " + boost::lexical_cast<wstring>(m_instanceId) +
                         L" -> " + logger->parse(hr));
        }
    }
    else
    {
        logger->debug(L"BrowserHelperObject::OnDisconnect() NativeBackground does not exist"); 
    }

    // BrowserHelperObject    
    logger->debug(L"BrowserHelperObject::OnDisconnect() release BHO");    
    if (m_webBrowser2 && m_isConnected) {
        hr = WebBrowserEvents2::DispEventUnadvise(m_webBrowser2); 
        if (hr == CONNECT_E_NOCONNECTION) { 
            // IE7 disconnects our connection points _before_ calling us with SetSite(NULL)
            logger->warn(L"BrowserHelperObject::OnDisconnect DispEventUnadvise bug");
            hr = S_OK; 
        } else if (FAILED(hr)) { 
            logger->error(L"BrowserHelperObject::OnDisconnect failed to unsink events"
                          L" -> " + logger->parse(hr));
        } else {
            logger->debug(L"BrowserHelperObject::OnDisconnect unsunk events");
        }
        m_isConnected = false;
    }
    this->DetachFromDocEvents();

    logger->debug(L"BrowserHelperObject::OnDisconnect() done");    

    return S_OK;
}


/**
 * Helper: MatchManifest -> IWebBrowser2 -> Manifest -> location -> (styles . scripts)
 */
std::pair<wstringvector, wstringvector> 
CBrowserHelperObject::MatchManifest(IWebBrowser2 *webBrowser2,
                                    const Manifest::pointer& manifest, 
                                    const wstring& location)
{
    // Do nothing for special urls
    if (!(location.find(L"http:")  == 0 ||
          location.find(L"https:") == 0 ||
          location.find(L"file:")  == 0 /*||
		  location.find(L"about:") == 0*/)) {
        logger->debug(L"BrowserHelperObject::MatchManifest boring url"
                      L" -> " + location);        
        return std::pair<wstringvector, wstringvector>();
    }

    // is this a html document?
    CComPtr<IDispatch> disp;
    webBrowser2->get_Document(&disp);
    if (!disp) {
        logger->debug(L"BrowserHelperObject::MatchManifest "
                      L"no document");
        return std::pair<wstringvector, wstringvector>();
    }
    CComQIPtr<IHTMLDocument2, &IID_IHTMLDocument2> htmlDocument2(disp);
    if (!htmlDocument2) {
        logger->debug(L"BrowserHelperObject::MatchManifest "
                      L"not an IHTMLDocument2");
        return std::pair<wstringvector, wstringvector>();
    }    

    // is this an iframe ?
    wstring locationtop = location;
    bool is_not_iframe = m_webBrowser2.IsEqualObject(webBrowser2);
    if (is_not_iframe) {
    } else {
        CComPtr<IDispatch> disp_parent;
        HRESULT hr = webBrowser2->get_Parent(&disp_parent);
        if (FAILED(hr) || !disp_parent) {
            logger->debug(L"BrowserHelperObject::MatchManifest not an iframe 1"
                          L" -> " + location);
            return std::pair<wstringvector, wstringvector>();
        }
        CComQIPtr<IHTMLDocument2, &IID_IHTMLDocument2> htmlDocument2_parent(disp_parent);
        if (!htmlDocument2_parent) {
            logger->debug(L"BrowserHelperObject::MatchManifest not an iframe 2"
                          L" -> " + location);
            return std::pair<wstringvector, wstringvector>();
        }
        CComBSTR bstr;
        htmlDocument2_parent->get_URL(&bstr);
        locationtop = bstr;
    }

    // check content_scripts.matches, content_scripts.all_frames
    std::pair<wstringvector, wstringvector> ret;
    Manifest::ContentScripts::const_iterator script = manifest->content_scripts.begin();
    for (; script != manifest->content_scripts.end(); script++) {

		// check for exclude_matches
		bool exclude = false;
		wstringvector::const_iterator exclude_match = script->exclude_matches.begin();
		for (; exclude_match != script->exclude_matches.end(); exclude_match++) {
			if (wstring_match_wild(*exclude_match, location)) {
				logger->debug(L"BrowserHelperObject::MatchManifest exclude "
							  L" -> " + *exclude_match + L" -> " + location);
				exclude = true;
			}
		}

		// check for matches and add if, and only if there are no excludes for this activation
        wstringvector::const_iterator match = script->matches.begin();
        for (; match != script->matches.end(); match++) {
            bool matches = wstring_match_wild(*match, location);
            if ((matches && is_not_iframe && !exclude) ||        // matches top-level ||
                (matches && script->all_frames && !exclude)) {   // matches iframe  
                ret.first.insert(ret.first.end(), script->css.begin(), script->css.end());
                ret.second.insert(ret.second.end(), script->js.begin(), script->js.end());
            }
        }
    }

    return ret;
}

void __stdcall CBrowserHelperObject::OnDownloadComplete()
{
    HRESULT hr;

    WebBrowser2Ptr webBrowser2(m_webBrowser2.p);
    if (!webBrowser2) {
        logger->debug(L"BrowserHelperObject::OnDownloadComplete2 "
                      L"failed to obtain IWebBrowser2");
        return;
    }

    CComBSTR bstr;
    hr = webBrowser2->get_LocationURL(&bstr);

    wstring location;
    if (FAILED(hr)) {
        location = L"";
    }else{
        location = bstr;
    }

    READYSTATE state;
    hr = webBrowser2->get_ReadyState(&state);
    if (FAILED(hr)) {
        state = READYSTATE_UNINITIALIZED;
    }

    logger->debug(L"CBrowserHelperObject::OnDownloadComplete"
                  L" -> " + location +
                  L" -> " + boost::lexical_cast<wstring>(state));

    // This is a heuristic to avoid invoking InitializeDocument for every DownloadComplete
    if (!location.empty() && (state == READYSTATE_INTERACTIVE || state == READYSTATE_COMPLETE))
    {
        hr = this->InitializeDocument(webBrowser2,
                location,
                L"OnDownloadComplete");
        if (FAILED(hr)) {
            logger->debug(L"BrowserHelperObject::OnDownloadComplete couldn't completely initialize document"
                    L" -> " + logger->parse(hr));
            return;
        }
    }else{
        logger->debug(L"BrowserHelperObject::OnDownloadComplete not initializing document");
    }
}

/**
 * Event: OnNavigateComplete2
 */
void __stdcall CBrowserHelperObject::OnNavigateComplete2(IDispatch *dispatch,
                                                         VARIANT   *url) 
{
    CComQIPtr<IWebBrowser2, &IID_IWebBrowser2> webBrowser2(dispatch);
    if (!webBrowser2) {
        logger->debug(L"BrowserHelperObject::OnNavigateComplete2 "
                      L"failed to obtain IWebBrowser2");
        return;
    }

    // VARIANT *url chops off file:\\\ for local filesystem
    CComBSTR bstr;
    webBrowser2->get_LocationURL(&bstr); 
    wstring location(bstr);
	if (location == L"" && url->bstrVal) { // get_LocationURL fails in the most egregious of ways
		location = url->bstrVal;
	}

    // Make COM server attachments available to browser
    HRESULT hr = this->OnAttachForgeExtensions(webBrowser2, location, L"OnNavigateComplete2");
    if (FAILED(hr)) {
        logger->debug(L"BrowserHelperObject::OnNavigateComplete2 attach did not complete successfully, initialization cancelled"
                L" -> " + logger->parse(hr));
        return;
    }
}


/**
 * Event: OnDocumentComplete
 */
void __stdcall CBrowserHelperObject::OnDocumentComplete(IDispatch *dispatch,
                                                        VARIANT   *url)
{
    Manifest::pointer manifest = _AtlModule.moduleManifest;

    CComQIPtr<IWebBrowser2, &IID_IWebBrowser2> webBrowser2(dispatch);
    if (!webBrowser2) {
        logger->debug(L"BrowserHelperObject::OnDocumentComplete "
                      L"failed to obtain IWebBrowser2");
        return;
    } 

    // VARIANT *url chops off file:\\\ for local filesystem
    CComBSTR bstr;
    webBrowser2->get_LocationURL(&bstr); 
    wstring location(bstr);
	if (location == L"" && url->bstrVal) { // get_LocationURL fails in the most egregious of ways
		location = url->bstrVal;
	}
	if (location == L"" && url->bstrVal == NULL) {
        logger->debug(L"BrowserHelperObject::OnDocumentComplete "
					  L"blank location, not interested"
                      L" -> " + manifest->uuid +
					  L" -> " + location);
		return;
	}

    logger->debug(L"BrowserHelperObject::OnDocumentComplete"
                  L" -> " + manifest->uuid +
				  L" -> " + wstring(url->bstrVal) +
                  L" -> " + location);

    HRESULT hr = this->InitializeDocument(webBrowser2,
            location,
            L"OnDocumentComplete");
    if (FAILED(hr)) {
        logger->debug(L"BrowserHelperObject::OnDocumentComplete couldn't completely initialize document"
                L" -> " + logger->parse(hr));
        return;
    }
}

/**
 * This method is idempotent to allow it to be used at various points during the loading of the
 * document. This is required because the order and occurrence of DWebBrowserEvents is largely
 * unpredictable.
 */
HRESULT CBrowserHelperObject::InitializeDocument(WebBrowser2Ptr webBrowser2,
        const wstring& location,
        const wstring& eventsource)
{
    HRESULT hr;

	logger->debug(L"BrowserHelperObject::InitializeDocument"
				  L" -> " + boost::lexical_cast<wstring>(webBrowser2.p) +
				  L" -> " + location +
				  L" -> " + eventsource);

    Manifest::pointer manifest = _AtlModule.moduleManifest;

    // match location against manifest 
    std::pair<wstringvector, wstringvector> match = this->MatchManifest(webBrowser2, manifest, location);
    if (match.first.size() == 0 && match.second.size() == 0) {
        logger->debug(L"BrowserHelperObject::InitializeDocument not interested"
                      L" -> " + manifest->uuid +
                      L" -> " + location);
        return S_OK;
    }

    // Make COM server attachments available to browser
    hr = this->OnAttachForgeExtensions(webBrowser2, location, eventsource);
    if (FAILED(hr)) {
        logger->debug(L"Attach did not complete successfully, initialization cancelled"
                L" -> " + logger->parse(hr));
        return hr;
    }

    // get IHTMLWindow2
    CComPtr<IDispatch> disp;
    hr = webBrowser2->get_Document(&disp);
    if (!disp) {
        logger->debug(L"BrowserHelperObject::InitializeDocument get_Document failed");
        return hr;
    }
    CComQIPtr<IHTMLDocument2, &IID_IHTMLDocument2> htmlDocument2(disp);
    if (!htmlDocument2) {
        logger->debug(L"BrowserHelperObject::InitializeDocument IHTMLDocument2 failed");
        return E_FAIL;
    }
    CComQIPtr<IHTMLWindow2, &IID_IHTMLWindow2> htmlWindow2;
    hr = htmlDocument2->get_parentWindow(&htmlWindow2);    
    if (!htmlWindow2) {
        logger->debug(L"BrowserHelperObject::InitializeDocument IHTMLWindow2 failed");
        return hr;
    }
    CComQIPtr<IDispatchEx> htmlWindow2Ex(htmlWindow2);
    if (!htmlWindow2Ex) {
        logger->debug(L"BrowserHelperObject::InitializeDocument IHTMLWindow2Ex failed");
        return E_FAIL;
    }

    CComPtr<IHTMLElement> documentActiveElement;
    hr = htmlDocument2->get_activeElement(&documentActiveElement);
    if (FAILED(hr) || !documentActiveElement) {
        logger->debug(L"BrowserHelperObject::InitializeDocument activeElement is not available, not injecting scripts"
                      L" -> " + logger->parse(hr));
        return E_FAIL;
    }

    CComBSTR activeElementTag;
    hr = documentActiveElement->get_tagName(&activeElementTag);
    if (FAILED(hr)) {
        logger->debug(L"BrowserHelperObject::InitializeDocument failed to determine active element tag"
                      L" -> " + logger->parse(hr));
    }else{
        logger->debug(L"BrowserHelperObject::InitializeDocument active element tag = " +
                wstring(activeElementTag));
    }

    if (!Attach::IsPropertyAttached(htmlWindow2Ex, FORGE_SCRIPT_INJECTION_TAG))
    {
        bool injectionFailed = false;

        // Inject styles
        wstringset dupes;
        HTMLDocument document(webBrowser2); 
        ScriptExtensions::scriptvector matches = m_scriptExtensions->read_styles(match.first);
        ScriptExtensions::scriptvector::const_iterator i = matches.begin();
        for (; i != matches.end(); i++) {
            if (dupes.find(i->first) != dupes.end()) {
                logger->debug(L"BrowserHelperObject::InitializeDocument already injected -> " + i->first);
                continue;
            }
            wstringpointer style = i->second;
            if (!style) {
                logger->debug(L"BrowserHelperObject::InitializeDocument invalid stylesheet -> " + i->first);
                continue;
            }
            HRESULT hr;
            hr = document.InjectStyle(style);
            if (FAILED(hr)) {
                logger->error(L"BrowserHelperObject::InitializeDocument failed to inject style"
                              L" -> " + i->first +
                              L" -> " + logger->parse(hr));
                injectionFailed = true;
                continue;
            }
            dupes.insert(i->first);
            logger->debug(L"BrowserHelperObject::InitializeDocument injected: " + i->first);
        }    

        // Inject scripts
        dupes.clear();
        matches = m_scriptExtensions->read_scripts(match.second);
        i = matches.begin();
        for (; i != matches.end(); i++) {
            if (dupes.find(i->first) != dupes.end()) {
                logger->debug(L"BrowserHelperObject::InitializeDocument already injected -> " + i->first);
                continue;
            }
            wstringpointer script = i->second;
            if (!script) {
                logger->debug(L"BrowserHelperObject::InitializeDocument invalid script -> " + i->first);
                continue;
            }
            HRESULT hr;
            //hr = document.InjectScript(script);
            //hr = document.InjectScriptTag(HTMLDocument::attrScriptType, i->first);
            CComVariant ret;
            hr = htmlWindow2->execScript(CComBSTR(script->c_str()), L"javascript", &ret);
            if (FAILED(hr)) {
                logger->error(L"BrowserHelperObject::InitializeDocument failed to inject script"
                              L" -> " + i->first +
                              L" -> " + logger->parse(hr));
                injectionFailed = true;
                continue;
            }
            dupes.insert(i->first);
            logger->debug(L"BrowserHelperObject::InitializeDocument injected"
                          L" -> " + location +
                          L" -> " + i->first);
        }

        if (!injectionFailed) {
            hr = Attach::ForgeScriptInjectionTag(manifest->uuid,
                    htmlWindow2Ex,
                    FORGE_SCRIPT_INJECTION_TAG);
            if (FAILED(hr)) {
                logger->error(L"BrowserHelperObject::InitializeDocument failed to attach script injection tag"
                              L" -> " + logger->parse(hr));
                return hr;
            }
        }else{
            logger->error(L"BrowserHelperObject::InitializeDocument failed to inject script and/or styles,"
                          L" not recording completion of injection in DOM");
            return E_FAIL;
        }

        // Register for events in the document to catch focus events, detach from the current doc first
        hr = this->DetachFromDocEvents();
        if (FAILED(hr)) {
            logger->debug(L"BrowserHelperObject::InitializeDocument failed to detach from doc events"
                    L" -> " + logger->parse(hr));
            return hr;
        }
        this->m_currentDocument = htmlDocument2;
        
        hr = HtmlDocEvents2::DispEventAdvise(this->m_currentDocument);
        if (FAILED(hr)) {
            logger->debug(L"BrowserHelperObject::InitializeDocument failed to attach to document events"
                          L" -> " + logger->parse(hr));
            return hr;
        }
    }else{
        logger->debug(L"BrowserHelperObject::InitializeDocument forge scripts already injected into document");
    }

    return S_OK;
}

/**
 * This method is idempotent to allow it to be used at various points during the loading of the
 * document. This is required because the order and occurrence of DWebBrowserEvents is largely
 * unpredictable.
 */
HRESULT CBrowserHelperObject::OnAttachForgeExtensions(WebBrowser2Ptr webBrowser2,
        const wstring& location,
        const wstring& eventsource)
{
    HRESULT hr;

	logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions"
				  L" -> " + boost::lexical_cast<wstring>(webBrowser2.p) +
				  L" -> " + location +
				  L" -> " + eventsource);

    Manifest::pointer manifest = _AtlModule.moduleManifest;

    // get interfaces
    CComPtr<IDispatch> disp;
    hr = webBrowser2->get_Document(&disp);
    if (!disp) {
        logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions get_Document failed");
        return hr;
    }
	logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions "
				  L" IDispatch -> " + boost::lexical_cast<wstring>(disp)); 
    CComQIPtr<IHTMLDocument2, &IID_IHTMLDocument2> htmlDocument2(disp);
    if (!htmlDocument2) {
        logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions IHTMLDocument2 failed");
        return E_FAIL;
    }
	logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions "
				  L" IHTMLDocument2 -> " + boost::lexical_cast<wstring>(htmlDocument2)); 
    CComQIPtr<IHTMLWindow2, &IID_IHTMLWindow2> htmlWindow2;
    hr = htmlDocument2->get_parentWindow(&htmlWindow2);    
    if (!htmlWindow2) {
        logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions IHTMLWindow2 failed");
        return hr;
    }
	logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions "
				  L" IHTMLWindow2 -> " + boost::lexical_cast<wstring>(htmlWindow2)); 
    CComQIPtr<IDispatchEx> htmlWindow2Ex(htmlWindow2);
    if (!htmlWindow2Ex) {
        logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions IHTMLWindow2Ex failed");
        return E_FAIL;
    }

    if (!Attach::IsPropertyAttached(htmlWindow2Ex, ACCESSIBLE_PROPERTY)) {
        // Attach NativeAccessible (forge.tabs.*)
        if (m_nativeAccessible) {
            logger->error(L"BrowserHelperObject::OnAttachForgeExtensions resetting nativeAccessible");
            m_nativeAccessible.reset();
        }
        m_nativeAccessible = NativeAccessible::pointer(new NativeAccessible(webBrowser2));
        hr = Attach::NativeTabs(htmlWindow2Ex, 
                                ACCESSIBLE_PROPERTY,
                                m_nativeAccessible.get());
        if (FAILED(hr)) {
            logger->error(L"BrowserHelperObject::OnAttachForgeExtensions "
                          L"failed to attach NativeAccessible"
                          L" -> " + logger->parse(hr));
            return hr;
        }
    }else{
        logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions "
                      L"NativeAccessible already attached");
    }

    // Attach NativeExtensions
    hr = Attach::NativeExtensions(manifest->uuid,
            htmlWindow2Ex, 
            EXTENSIONS_PROPERTY, 
            m_instanceId,
            location,
            &m_nativeExtensions.p); // "T** operator&() throw()" asserts on p==NULL
    if (FAILED(hr)) {
        logger->error(L"BrowserHelperObject::OnAttachForgeExtensions "
                L"failed to attach NativeExtensions"
                L" -> " + logger->parse(hr));
        return hr;
    }
    logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions "
            L"attached NativeExtensions");

    // Attach NativeMessaging
    hr = Attach::NativeMessaging(manifest->uuid,
            htmlWindow2Ex, 
            MESSAGING_PROPERTY, 
            m_instanceId,
            &m_nativeMessaging.p); // "T** operator&() throw()" asserts on p==NULL
    if (FAILED(hr)) {
        logger->error(L"BrowserHelperObject::OnAttachForgeExtensions "
                L"failed to attach NativeMessaging"
                L" -> " + logger->parse(hr));
        return hr;
    }

    logger->debug(L"BrowserHelperObject::OnAttachForgeExtensions "
            L"attached NativeMessaging");

    return S_OK;
}

void __stdcall CBrowserHelperObject::OnFocusIn(IDispatch *idispatch)
{
    logger->debug(L"CBrowserHelperObject::OnFocusIn"
                  L" -> " + boost::lexical_cast<wstring>(this->m_instanceId));

    DWORD focused = OLECMDIDF_WINDOWSTATE_ENABLED | OLECMDIDF_WINDOWSTATE_USERVISIBLE;

    this->OnWindowStateChanged(focused, focused);
}

/**
 * Event: OnWindowStateChanged
 */
void __stdcall CBrowserHelperObject::OnWindowStateChanged(DWORD flags, DWORD mask)
{
    CComBSTR url;
    m_webBrowser2->get_LocationURL(&url);
    CComBSTR title;
    m_webBrowser2->get_LocationName(&title);
    logger->debug(L"CBrowserHelperObject::OnWindowStateChanged" 
                  L" -> " + boost::lexical_cast<wstring>(flags) +
                  L" -> " + boost::lexical_cast<wstring>(m_instanceId) +
                  L" -> " + wstring(url) +
                  L" -> " + wstring(title));
        
    bool focused = false;
    switch (flags) {
    case OLECMDIDF_WINDOWSTATE_USERVISIBLE:   break;
    case OLECMDIDF_WINDOWSTATE_ENABLED:       break;
    case 3:   // MSDN docs are _WRONG_ ... no they aren't, it's a bitmask
        focused = true;
        break;
    }

	//if this is a blank page, we don't make it to be the active tab 
	wstring location(url);
	if (location.find(L"about:blank") == 0) {
		focused = false;
	}
	
    m_tabInfo.id     = m_instanceId;
    m_tabInfo.active = focused;
    m_tabInfo.url    = url;
    m_tabInfo.title  = title;

    // update tab info
    if (m_nativeMessaging) {
        HRESULT hr;
        hr = m_nativeMessaging->tabs_set(CComBSTR(_AtlModule.moduleManifest->uuid.c_str()), 
                                         m_tabInfo.id, 
                                         CComBSTR(m_tabInfo.url.c_str()), 
                                         CComBSTR(m_tabInfo.title.c_str()), 
                                         m_tabInfo.active);
        if (FAILED(hr)) {
            logger->error(L"BrowserHelperObject::OnWindowStateChanged "
                          L"tabs_Set failed "
                          L" -> " + boost::lexical_cast<wstring>(m_tabInfo.id) +
                          L" -> " + m_tabInfo.url +
                      L" -> " + m_tabInfo.title + 
                          L" -> " + boost::lexical_cast<wstring>(m_tabInfo.active) +
                          L" -> " + logger->parse(hr));
            return;
        }
    }
}


/**
 * IDispatchImpl (for FrameProxy)
 */
STDMETHODIMP CBrowserHelperObject::Invoke(DISPID dispid, 
                                          REFIID riid, 
                                          LCID   lcid, 
                                          WORD   flags, 
                                          DISPPARAMS *params, 
                                          VARIANT    *result, 
                                          EXCEPINFO  *excepinfo, 
                                          UINT       *arg) 
{
    if (dispid == DISPID_WINDOWSTATECHANGED) {
        DWORD dflags = params->rgvarg[1].intVal;
        DWORD valid  = params->rgvarg[0].intVal;
        
        // check whether the event is raised because tab became active
        if ((valid & OLECMDIDF_WINDOWSTATE_USERVISIBLE)  != 0 && 
            (dflags & OLECMDIDF_WINDOWSTATE_USERVISIBLE) != 0 &&
            (valid & OLECMDIDF_WINDOWSTATE_ENABLED)      != 0 && 
            (dflags & OLECMDIDF_WINDOWSTATE_ENABLED)     != 0) {
            //logger->debug(L"CBrowserHelperObject::Invoke m_frameProxy->SetCurrent");
            if (m_frameProxy) {
                m_frameProxy->SetCurrent();
            }
        }
	} else if (dispid == DISPID_COMMANDSTATECHANGE) {
		// if we hover over a window that wasn't active and click on the extension icon
		if (m_frameProxy) {
            m_frameProxy->SetCurrent();
        }
	}

    // forward invocation
    HRESULT hr;
    hr = WebBrowserEvents2::Invoke(dispid, riid, lcid, flags, params, 
                                   result, excepinfo, arg);
    if (FAILED(hr)) {
        logger->error(L"CBrowserHelperObject::Invoke failed"
                      L" -> " + logger->parse(hr));
    }

    return hr;
}

