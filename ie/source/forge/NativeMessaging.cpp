#include "stdafx.h"
#include "NativeMessaging.h"


/**
 * Construction
 */
CNativeMessaging::CNativeMessaging()
{
}


/** 
 * Destruction
 */
CNativeMessaging::~CNativeMessaging()
{
    logger->debug(L"CNativeMessaging::~CNativeMessaging");
}

HRESULT CNativeMessaging::FinalConstruct()
{
    return S_OK;
}

void CNativeMessaging::FinalRelease()
{
    logger->debug(L"CNativeMessaging::FinalRelease");
}


/**
 * InterfaceSupportsErrorInfo
 */
STDMETHODIMP CNativeMessaging::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* const arr[] = 
	{
		&IID_INativeMessaging
	};

	for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}


/**
 * Method: NativeMessaging::tabs_set
 *
 * Called by the browser whenever the active tab changes 
 *
 * @param uuid
 * @param tabInfo
 */
STDMETHODIMP CNativeMessaging::tabs_set(BSTR uuid, 
                                        UINT instanceId,
                                        BSTR url, BSTR title, BOOL focused)
{
    logger->debug(L"NativeMessaging::tabs_set"
                  L" -> " + wstring(uuid) +
                  L" -> " + boost::lexical_cast<wstring>(instanceId) +
                  L" -> " + wstring(url) +
                  L" -> " + wstring(title) +
                  L" -> " + boost::lexical_cast<wstring>(focused));

    // Store the tab data for later retrieval
    TabMap::const_iterator findIter = this->m_tabs.find(instanceId);
    if (findIter == this->m_tabs.end()) {
        Tab tab(instanceId, focused ? true : false, url, title);
        this->m_tabs.insert(std::pair<int, Tab>(instanceId, tab));
    }

    if (focused) {
        m_activeTab.id       = instanceId;
        m_activeTab.url      = url;
        m_activeTab.title    = title;
        m_activeTab.selected = focused ? true : false;

        // Invoke the active tab callback, if necessary
        if (m_activeTabCallback) {
           UINT tabId = 0; 
           HRESULT hr = this->tabs_active(uuid, m_activeTabCallback.p, &tabId);
           if (FAILED(hr)) {
               logger->error(L"NativeMessaging::tabs_set "
                       L"failed to invoke active tab callback"
                       L" -> " + logger->parse(hr));
               return hr;
           }
        }
    }
	//if this tab isn't focused, set the active tab to no one
	//to make sure BE doesn't decide this tab as the active tab anymore
	//until this tab is selected again
	else if (m_activeTab.id == instanceId) {
		m_activeTab.id = -1;		
	}

    return S_OK;
}

STDMETHODIMP CNativeMessaging::active_tab_listen(BSTR uuid, IDispatch *callback)
{
    logger->debug(L"NativeMessaging::active_tab_list"
                  L" -> " + wstring(uuid) +
                  L" -> " + boost::lexical_cast<wstring>(callback));
    this->m_activeTabCallback = CComPtr<IDispatch>(callback);

    return S_OK;
}

/**
 * Method: NativeMessaging::tabs_active
 *
 * @param uuid
 *
 * @returns Tab
 */
STDMETHODIMP CNativeMessaging::tabs_active(BSTR uuid, IDispatch *callback, UINT *out_tabId)
{
    HRESULT hr;

    logger->debug(L"NativeMessaging::tabs_active"
                  L" -> " + wstring(uuid) +
                  L" -> " + boost::lexical_cast<wstring>(callback));

    *out_tabId = m_activeTab.id;

    if (!callback) {
        logger->error(L"NativeMessaging::tabs_active no callback");
        return S_OK;
    }

    CComPtr<ITypeInfo> tabT;
    hr = ::CreateDispTypeInfo(&Tab::Interface, LOCALE_SYSTEM_DEFAULT,
                              &tabT);
    if (FAILED(hr) || !tabT) {
        logger->error(L"NativeMessaging::tabs_active "
                      L"failed to create tabT"
                      L" -> " + logger->parse(hr));
        return hr;
    }
    CComPtr<IUnknown> tabI;
    hr = ::CreateStdDispatch(NULL, &m_activeTab, tabT, &tabI);
    if (FAILED(hr) || !tabI) {
        logger->error(L"NativeMessaging::tabs_active "
                      L"failed to create tabI"
                      L" -> " + logger->parse(hr));
        return hr;
    }

    hr = CComDispatchDriver(callback).Invoke1((DISPID)0, &CComVariant(tabI));
    if (FAILED(hr)) {    
        logger->error(L"NativeMessaging::tabs_active "
                      L"failed to invoke callback"
                      L" -> " + logger->parse(hr));
    }

    return hr;
}

STDMETHODIMP CNativeMessaging::get_tabs(BSTR uuid, IDispatch *success, IDispatch *error)
{
    const wchar_t * const GENERIC_ERR_MSG = L"{ 'message' : 'failed to retrieve tabs' }";

    HRESULT hr = S_OK;

    CComPtr<ITypeInfo> tabT;
    hr = ::CreateDispTypeInfo(&Tab::Interface, LOCALE_SYSTEM_DEFAULT, &tabT);
    if (FAILED(hr) || !tabT) {
        logger->error(L"NativeMessagging::get_tabs "
                      L"failed to create tabT"
                      L" -> " + logger->parse(hr));
        return CComDispatchDriver(error).Invoke1((DISPID)0, &CComVariant(GENERIC_ERR_MSG));
    }


    // Build up an array of tabs
    CComSafeArray<VARIANT> tabsArray;
    hr = tabsArray.Create((ULONG)this->m_tabs.size());
    if ( FAILED(hr) ) {
        logger->error(L"NativeMessaging::get_tabs "
                      L"failed to create tabsArray"
                      L" -> " + logger->parse(hr));
        return CComDispatchDriver(error).Invoke1((DISPID)0, &CComVariant(GENERIC_ERR_MSG));
    }

    try {
        int index = 0;
        for (TabMap::iterator i = this->m_tabs.begin();
                i != this->m_tabs.end();
                ++i)
        {
            Tab &tab = i->second;

            CComPtr<IUnknown> tabI;
            hr = ::CreateStdDispatch(NULL, &tab, tabT, &tabI);
            if (FAILED(hr) || !tabI) {
                logger->error(L"NativeMessaging::get_tabs "
                              L"failed to create tabI:"
                              L" -> " + boost::lexical_cast<wstring>(tab.id) +
                              L" -> " + tab.url +
                              L" -> " + logger->parse(hr));
                return CComDispatchDriver(error).Invoke1((DISPID)0, &CComVariant(GENERIC_ERR_MSG));
            }

            CComVariant tabVariant(tabI);
            hr = tabsArray.SetAt(index, tabVariant);
            if (FAILED(hr)) {
                logger->error(L"NativeMessaging::get_tabs "
                        L"failed to store tab in array:"
                        L" -> " + boost::lexical_cast<wstring>(tab.id) +
                        L" -> " + tab.url +
                        L" -> " + logger->parse(hr));
                return CComDispatchDriver(error).Invoke1((DISPID)0, &CComVariant(GENERIC_ERR_MSG));
            }
            index++;
        }
    }catch (const CAtlException &e) {
        logger->error(L"NativeMessaging::get_tabs "
                      L"failed to populate tabsArray"
                      L" -> " + logger->parse(e.m_hr));
        return CComDispatchDriver(error).Invoke1((DISPID)0, &CComVariant(GENERIC_ERR_MSG));
    }

    SAFEARRAY *rawTabsArray = tabsArray.Detach();

    logger->debug(L"NativeMessaging::get_tabs "
                  L"returning " + boost::lexical_cast<wstring>(this->m_tabs.size()) + L" tab(s)");
    return CComDispatchDriver(success).Invoke1((DISPID)0, &CComVariant(rawTabsArray));
}

/**
 * Method: NativeMessaging::get_tab
 *
 * @param uuid
 *
 * @returns Tab
 */
STDMETHODIMP CNativeMessaging::get_tab(BSTR uuid, IDispatch *callback)
{
    HRESULT hr;

    logger->debug(L"NativeMessaging::get_tab"
                  L" -> " + wstring(uuid) +
                  L" -> " + boost::lexical_cast<wstring>(callback));

    if (!callback) {
        logger->error(L"NativeMessaging::get_tab no callback");
        return S_OK;
    }

    CComPtr<ITypeInfo> tabT;
    hr = ::CreateDispTypeInfo(&Tab::Interface, LOCALE_SYSTEM_DEFAULT,
                              &tabT);
    if (FAILED(hr) || !tabT) {
        logger->error(L"NativeMessaging::get_tab "
                      L"failed to create tabT"
                      L" -> " + logger->parse(hr));
        return hr;
    }
    CComPtr<IUnknown> tabI;
	
	Tab tab;
	tab.id = m_instanceId;
	// TO-DO: in case we need to use the tab URL in the future, firstly, 
	// we should find correct tab with tabId above.
	//tab.url = m_activeTab.url;

	hr = ::CreateStdDispatch(NULL, &tab, tabT, &tabI);
    if (FAILED(hr) || !tabI) {
        logger->error(L"NativeMessaging::get_tab "
                      L"failed to create tabI"
                      L" -> " + logger->parse(hr));
        return hr;
    }

    hr = CComDispatchDriver(callback).Invoke1((DISPID)0, &CComVariant(tabI));
    if (FAILED(hr)) {    
        logger->error(L"NativeMessaging::get_tab "
                      L"failed to invoke callback"
                      L" -> " + logger->parse(hr));
    }

    return hr;
}

/**
 * Method: NativeMessaging::load
 *
 * @param uuid
 * @param instance
 */
STDMETHODIMP CNativeMessaging::load(BSTR uuid, unsigned int instanceId)
{
    /*logger->debug(L"CNativeMessaging::load"
                  L" -> " + wstring(uuid) +
                  L" -> " + boost::lexical_cast<wstring>(instanceId));*/
    m_instanceId = instanceId;
    m_clients[uuid].insert(instanceId);

    return S_OK;
}


/**
 * Method: NativeMessaging::unload
 *
 * @param uuid
 * @param instance
 */
struct delete_callback { 
    delete_callback(UINT tabId) : tabId(tabId) {}
    UINT tabId;
    bool operator()(Callback::pointer callback) const {
        logger->debug(L"CNativeMessaging::unload checking callback "
                      L" -> " + boost::lexical_cast<wstring>(tabId) +
                      L" -> " + callback->toString());
        if (callback->tabId == tabId) {
            logger->debug(L"\terased");
            return true;
        }
        return false;
    }
};

void CNativeMessaging::unloadHelper(BSTR uuid, unsigned int instanceId, Callback::map& callbacks, const wstring& desc)
{
    // This helper function is to delete a callback entry from given array of callbacks.
    // Its done fairly complicated way to prevent crashes seen on Win8/IE11 where "callbacks[uuid].erase(it)"
    // may not return immediately but rather cause recursive calls to unload/unloadHelper (even multi-level)
    // causing memory corruption if the iterator attempts to continue with iteration after each erase.
    // The safe apprach is to forget about anything being done once the erase() returns and start from the beggining.

    Callback::vector::iterator it = callbacks[uuid].begin();
    for( ; it != callbacks[uuid].end();)
    {
        if (it->use_count())
        {
            if ((*it)->tabId == instanceId)
            {
//                wstring wid = (*it)->toString(); // need to keep local copy as the erase() wipes it out
//                logger->debug(L"CNativeMessaging::unload(" + desc + L") before delete: use_count=" +  boost::lexical_cast<wstring>(it->use_count()) + L", instanceId=" +  boost::lexical_cast<wstring>(instanceId) + L": " + wid);
                callbacks[uuid].erase(it);
//                logger->debug(L"CNativeMessaging::unload(" + desc + L") after delete - restarting the delete loop: instanceId=" +  boost::lexical_cast<wstring>(instanceId) + L": " + wid);

                it = callbacks[uuid].begin(); // start from the beginning, there could be recursive call which deleted something in between!
            }
            else
            {
                // skip this one, its not the tabId we are after
                ++it;
            }
        }
        else
        {
            // pending delete (ref_counter is zero) -> don't touch it, just skip it
            ++it;
//            logger->debug(L"CNativeMessaging::unload(" + desc + L") pending delete: " +  boost::lexical_cast<wstring>(instanceId));
        }
    }
}

STDMETHODIMP CNativeMessaging::unload(BSTR uuid, unsigned int instanceId)
{
    logger->debug(L"CNativeMessaging::unload"
                  L" -> " + wstring(uuid) +
                  L" -> " + boost::lexical_cast<wstring>(instanceId));

    // clean up any callbacks registered for this instance
    unloadHelper(uuid, instanceId, fg_callbacks, L"fg_callbacks");
    unloadHelper(uuid, instanceId, bg_callbacks, L"bg_callbacks");

    // stop tracking the tab object
    m_tabs.erase(instanceId);

    m_clients[uuid].erase(instanceId);
    if (m_clients[uuid].empty()) {
        logger->debug(L"CNativeMessaging::unload "
                      L"shutting down extension"
                      L" -> " + wstring(uuid));
        this->bg_callbacks.erase(uuid);
        this->fg_callbacks.erase(uuid);
        m_clients.erase(uuid);
        logger->debug(L"CNativeMessaging::unload done");
    }

    logger->debug(L"CNativeMessaging::unload clients remaining: " +
                  boost::lexical_cast<wstring>(m_clients[uuid].size()));

    if (m_clients.empty()) {
        return this->shutdown();
    } 

    return S_OK;
}


/**
 * Helper: shutdown
 */
HRESULT CNativeMessaging::shutdown()
{
    logger->debug(L"CNativeMessaging::shutdown "
                  L" -> " + boost::lexical_cast<wstring>(m_dwRef));

    this->bg_callbacks.clear();
    this->fg_callbacks.clear();
    
    return S_OK;
}


/**
 * Method: fg_listen
 *
 * FG <= BG
 */
STDMETHODIMP CNativeMessaging::fg_listen(BSTR _uuid, UINT tabId,
                                         BSTR _type, 
                                         IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type);

    /*logger->debug(L"CNativeMessaging::fg_listen"
                  L" -> " + uuid +
                  L" -> " + boost::lexical_cast<wstring>(tabId) +
                  L" -> " + type +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));*/

    this->fg_callbacks[uuid].push_back
        (Callback::pointer(new Callback(tabId, type, callback, error)));

    return S_OK;
}


/**
 * Method: fg_broadcast
 *
 * FG => FG(*)
 */
STDMETHODIMP CNativeMessaging::fg_broadcast(BSTR _uuid, 
                                            BSTR _type, BSTR _content, 
                                            IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type), content(_content);

    /*logger->debug(L"CNativeMessaging::fg_broadcast"
                  L" -> " + uuid +
                  L" -> " + type +
                  L" -> " + content +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));*/

    HRESULT hr;
    Callback::vector v = fg_callbacks[uuid];
    for (Callback::vector::const_iterator i = v.begin(); i != v.end(); i++) {
        Callback::pointer fg_callback = *i;
        if (fg_callback->type == L"*" || 
            fg_callback->type == type) {
            hr = fg_callback->Dispatch(content, callback);
        }
    }    

    return S_OK;
}


/**
 * Method: fg_toFocussed
 *
 * FG => FG(1)
 */
STDMETHODIMP CNativeMessaging::fg_toFocussed(BSTR _uuid, 
                                             BSTR _type, BSTR _content, 
                                             IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type), content(_content);

    /*logger->debug(L"CNativeMessaging::fg_toFocussed"
                  L" -> " + uuid +
                  L" -> " + type +
                  L" -> " + content +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));*/

    HRESULT hr;
    Callback::vector v = fg_callbacks[uuid];
    for (Callback::vector::const_iterator i = v.begin(); i != v.end(); i++) {
        Callback::pointer fg_callback = *i;
        if (fg_callback->tabId != m_activeTab.id) continue; 
        if (fg_callback->type == L"*" || 
            fg_callback->type == type) {
            hr = fg_callback->Dispatch(content, callback);
        }
    }    

    return S_OK;
}

/**
 * Method: fg_broadcastBackround
 *
 * FG => BG
 */
STDMETHODIMP CNativeMessaging::fg_broadcastBackground(BSTR _uuid, 
                                                      BSTR _type, BSTR _content, 
                                                      IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type), content(_content);

    /*logger->debug(L"CNativeMessaging::fg_broadcastBackground"
                  L" -> " + uuid +
                  L" -> " + type +
                  L" -> " + content +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));*/

    HRESULT hr;
    Callback::vector v = bg_callbacks[uuid];
    for (Callback::vector::const_iterator i = v.begin(); i != v.end(); i++) {
        Callback::pointer bg_callback = *i;
        if (bg_callback->type == L"*" && type == L"bridge") continue;
        if (bg_callback->type == L"*" || 
            bg_callback->type == type) {
            hr = bg_callback->Dispatch(content, callback);
        }
    }

    return S_OK;
}


/**
 * Method: bg_listen -> type -> (String -> (String))
 *
 * BG <= FG
 */
STDMETHODIMP CNativeMessaging::bg_listen(BSTR _uuid, 
                                         BSTR _type, 
                                         IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type);

    /*logger->debug(L"CNativeMessaging::bg_listen"
                  L" -> " + uuid +
                  L" -> " + type +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));*/

    this->bg_callbacks[uuid].push_back
        (Callback::pointer(new Callback(type, callback, error)));

    return S_OK;
}


/**
 * Method: bg_broadcast
 *
 * BG => FG(*)
 */
STDMETHODIMP CNativeMessaging::bg_broadcast(BSTR _uuid, 
                                            BSTR _type, BSTR _content, 
                                            IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type), content(_content);
    /*logger->debug(L"CNativeMessaging::bg_broadcast"
                  L" -> " + uuid +
                  L" -> " + type +
                  L" -> " + content +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));*/

    HRESULT hr;
    Callback::vector v = fg_callbacks[uuid];
    for (Callback::vector::const_iterator i = v.begin(); i != v.end(); i++) {
        Callback::pointer fg_callback = *i;
        if (fg_callback->type == L"*" || 
            fg_callback->type == type) {
            hr = fg_callback->Dispatch(content, callback);
        }
    }    

    return S_OK;
}


/**
 * Method: bg_toFocussed
 *
 * BG => FG(1)
 */
STDMETHODIMP CNativeMessaging::bg_toFocussed(BSTR _uuid, 
                                             BSTR _type, BSTR _content, 
                                             IDispatch *callback, IDispatch *error)
{
    wstring uuid(_uuid), type(_type), content(_content);
    logger->debug(L"CNativeMessaging::bg_toFocussed"
                  L" -> " + uuid +
                  L" -> " + type +
                  L" -> " + content +
                  L" -> m_activeTab.id:" + boost::lexical_cast<wstring>(m_activeTab.id) +
                  L" -> m_activeTab.selected:" + boost::lexical_cast<wstring>(m_activeTab.selected) +
                  L" -> m_activeTab.url:" + boost::lexical_cast<wstring>(m_activeTab.url) +
                  L" -> m_activeTab.title:" + boost::lexical_cast<wstring>(m_activeTab.title) +
                  L" -> " + boost::lexical_cast<wstring>(callback) +
                  L" -> " + boost::lexical_cast<wstring>(error));
    
    HRESULT hr;
    Callback::vector v = fg_callbacks[uuid];
    for (Callback::vector::const_iterator i = v.begin(); i != v.end(); i++) {
        Callback::pointer fg_callback = *i;
        logger->debug(L"CNativeMessaging::bg_toFocussed callback tabId:"
                  L" -> " + boost::lexical_cast<wstring>(fg_callback->tabId));
        if (fg_callback->tabId != m_activeTab.id) {
            logger->debug(L"CNativeMessaging::bg_toFocussed callback skip...");
            continue; 
        }
        if (fg_callback->type == L"*" || 
            fg_callback->type == type) {
            logger->debug(L"CNativeMessaging::bg_toFocussed callback --> Dispatch");
            hr = fg_callback->Dispatch(content, callback);
        }
    }    

    return S_OK;
}
