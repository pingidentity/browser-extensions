#include "stdafx.h"
#include "NativeExtensions.h"
#include <generated/Forge_i.h> /* for: */
#include "dllmain.h"           /*   _AtlModule */
#include "Preferences.h"
#include "HTTP.h"
#include "NativeMessagingTypes.h"
#include "AccessibleBrowser.h"
#include "NotificationWindow.h"


/**
 * Construction
 */
CNativeExtensions::CNativeExtensions()
{
    /*#ifdef DEBUG
    logger->output(L"z:\\forge_com.log");
#else
    logger->output(L"forge_com.log");
    #endif*/ /* DEBUG */
}

CNativeExtensions::~CNativeExtensions()
{
    logger->debug(L"CNativeExtensions::~CNativeExtensions");
}


/** 
 * Destruction
 */
HRESULT CNativeExtensions::FinalConstruct()
{
    return S_OK;
}

void CNativeExtensions::FinalRelease()
{
    logger->debug(L"CNativeExtensions::FinalRelease");
}


/**
 * InterfaceSupportsErrorInfo
 */
STDMETHODIMP CNativeExtensions::InterfaceSupportsErrorInfo(REFIID riid)
{
    static const IID* const arr[] = {
        &IID_INativeExtensions
    };

    for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        if (InlineIsEqualGUID(*arr[i],riid))
            return S_OK;
    }
    return S_FALSE;
}


/**
 * Method: NativeExtensions::log
 * 
 * @param uuid 
 * @param message
 */
STDMETHODIMP CNativeExtensions::log(BSTR uuid, BSTR message)
{
    if (_AtlModule.moduleManifest->logging.console) {
        logger->debug(L"[" + wstring(uuid) + L"] " + wstring(message));
    } 
    return S_OK;
}


/**
 * Method: NativeExtensions::prefs_get
 *
 * @param name
 * @param success
 * @param error  TODO
 */
STDMETHODIMP CNativeExtensions::prefs_get(BSTR uuid,
                                          BSTR name, 
                                          IDispatch *success, 
                                          IDispatch *error)
{
    HRESULT hr;

    logger->debug(L"NativeExtensions::prefs_get"
                  L" -> " + wstring(uuid) + 
                  L" -> " + wstring(name) + 
                  L" -> " + boost::lexical_cast<wstring>(success) +
                  L" -> " + boost::lexical_cast<wstring>(error));

    wstring value = Preferences(uuid).get(name);

    logger->debug(L"NativeExtensions::prefs_get"
                  L" -> " + wstring_limit(value));

    CComQIPtr<IDispatchEx> dispatch(success);
    hr = CComDispatchDriver(dispatch).Invoke1((DISPID)0,
                                              &CComVariant(value.c_str()));
    if (FAILED(hr)) {
        logger->debug(L"NativeExtensions::prefs_get failed"
                      L" -> " + wstring(name) +
                      L" -> " + logger->parse(hr));
        return hr;
    }

    return S_OK;
}


/**
 * Method: NativeExtensions::prefs_set
 *
 * @param name
 * @param value
 * @param success
 * @param error TODO
 */
STDMETHODIMP CNativeExtensions::prefs_set(BSTR uuid, BSTR name, BSTR value, 
                                          IDispatch *success, 
                                          IDispatch *error)
{
    HRESULT hr;

    logger->debug(L"NativeExtensions::prefs_set"
                  L" -> " + wstring(uuid) + 
                  L" -> " + wstring(name) + 
                  L" -> " + wstring_limit(value) +
                  L" -> " + boost::lexical_cast<wstring>(success) +
                  L" -> " + boost::lexical_cast<wstring>(error));

    wstring ret = Preferences(uuid).set(name, value);
    logger->debug(L"NativeExtensions::prefs_set"
                  L" -> " + wstring_limit(ret));

    CComQIPtr<IDispatchEx> dispatch(success);
    hr = CComDispatchDriver(dispatch).Invoke1((DISPID)0,
                                              &CComVariant(ret.c_str()));
    if (FAILED(hr)) {
        logger->debug(L"NativeExtensions::prefs_set failed"
                      L" -> " + wstring(name) +
                      L" -> " + wstring_limit(ret) +
                      L" -> " + logger->parse(hr));
        return hr;
    }

    return S_OK;
}

/**
 * Method: NativeExtensions::prefs_getSync
 * Synchronous implementation of prefs.get
 * @param name
 */
STDMETHODIMP CNativeExtensions::prefs_getSync(BSTR uuid,
                                              BSTR name)
{
    HRESULT hr;

    logger->debug(L"NativeExtensions::prefs_getSync"
                  L" -> " + wstring(uuid) +
                  L" -> " + wstring(name));

    wstring value = Preferences(uuid).get(name);

    return value;
}


/**
 * Method: NativeExtensions::prefs_setSync
 *
 * @param name
 * @param value
 */
STDMETHODIMP CNativeExtensions::prefs_set(BSTR uuid, BSTR name, BSTR value)
{
    logger->debug(L"NativeExtensions::prefs_setSync"
                  L" -> " + wstring(uuid) +
                  L" -> " + wstring(name) +
                  L" -> " + wstring_limit(value));

    Preferences(uuid).set(name, value);

    return S_OK;
}


/**
 * Method: NativeExtensions::prefs_clearSync
 *
 * @param uuid
 * @param name
 */
STDMETHODIMP CNativeExtensions::prefs_clearSync(BSTR uuid, BSTR name)
{
    logger->debug(L"NativeExtensions::prefs_clearSync"
                  L" -> " + wstring(uuid) +
                  L" -> " + wstring(name));

    if (wstring(name) == L"*") {
        Preferences(uuid).clear();
    } else {
        Preferences(uuid).clear(name);
    }

    return S_OK;
}


/**
 * Method: NativeExtensions::prefs_keys
 *
 * @param uuid
 * @param success
 * @param error 
 */
STDMETHODIMP CNativeExtensions::prefs_keys(BSTR uuid, 
                                           IDispatch *success, IDispatch *error)
{
    HRESULT hr;

    logger->debug(L"NativeExtensions::prefs_keys"
                  L" -> " + wstring(uuid) + 
                  L" -> " + boost::lexical_cast<wstring>(success) +
                  L" -> " + boost::lexical_cast<wstring>(error));

    wstringvector keys = Preferences(uuid).keys();
    wstring json = keys.size()
        ? L"\"['" + boost::algorithm::join(keys, L"', '") + L"']\""
        : L"\"[]\"";

    hr = CComDispatchDriver(success).Invoke1((DISPID)0, 
                                             &CComVariant(json.c_str()));
    if (FAILED(hr)) {
        logger->debug(L"NativeExtensions::prefs_all failed"
                      L" -> " + logger->parse(hr));
        return CComDispatchDriver(error)
            .Invoke1((DISPID)0, &CComVariant(L"'failed to enumerate keys'"));
    }

    return S_OK;
}


/**
 * Method: NativeExtensions::prefs_all
 *
 * @param uuid
 * @param success
 * @param error 
 */
STDMETHODIMP CNativeExtensions::prefs_all(BSTR uuid, 
                                          IDispatch *success, IDispatch *error)
{
    HRESULT hr;

    logger->debug(L"NativeExtensions::prefs_all"
                  L" -> " + wstring(uuid) + 
                  L" -> " + boost::lexical_cast<wstring>(success) +
                  L" -> " + boost::lexical_cast<wstring>(error));

    wstringmap all = Preferences(uuid).all();

    hr = CComDispatchDriver(success).Invoke1((DISPID)0, 
                                              &CComVariant(L"{}"));
    if (FAILED(hr)) {
        logger->error(L"NativeExtensions::prefs_all failed"
                      L" -> " + logger->parse(hr));
        CComDispatchDriver(error)
            .Invoke1((DISPID)0, &CComVariant(L"'failed to enumerate keys'"));
        return hr;
    }

    return S_OK;
}


/**
 * Method: NativeExtensions::prefs_clear
 *
 * @param uuid
 * @param name
 * @param success
 * @param error 
 */
STDMETHODIMP CNativeExtensions::prefs_clear(BSTR uuid, BSTR name,
                                            IDispatch *success, IDispatch *error)
{
    logger->debug(L"NativeExtensions::prefs_clear"
                  L" -> " + wstring(uuid) + 
                  L" -> " + wstring(name) + 
                  L" -> " + boost::lexical_cast<wstring>(success) +
                  L" -> " + boost::lexical_cast<wstring>(error));

    bool ret = (wstring(name) == L"*") 
        ? Preferences(uuid).clear() 
        : Preferences(uuid).clear(name);
    if (ret) {
        CComDispatchDriver(success).Invoke0((DISPID)0);
    } else {
        logger->error(L"NativeExtensions::prefs_clear failed"
                      L" -> " + wstring(name));
        wstring message = L"failed to clear key: " + wstring(name);
        CComDispatchDriver(error).Invoke1((DISPID)0, &CComVariant(message.c_str()));
    }

    return S_OK;
}


/**
 * Method: NativeExtensions::getURL
 *
 * @param url
 * @param out_url
 */
STDMETHODIMP CNativeExtensions::getURL(BSTR url, BSTR *out_url)
{
    bfs::wpath path = 
        _AtlModule.modulePath / bfs::wpath(L"src") / bfs::wpath(url);

    wstring ret = L"file:///" + wstring_replace(path.wstring(), '\\', '/');
    *out_url = ::SysAllocString(ret.c_str()); // TODO leak

    return S_OK;
}


/**
 * Method: NativeExtensions::xhr
 *
 * @param method
 * @param url
 * @param data
 * @param success
 * @param error
 */
STDMETHODIMP CNativeExtensions::xhr(BSTR method, BSTR url, BSTR data, BSTR contentType, BSTR headers, 
                                    IDispatch *success, 
                                    IDispatch *error)
{
    // TODO headers

    HRESULT hr;

    logger->debug(L"NativeExtensions::xhr "
                  L" -> " + wstring(method) + 
                  L" -> " + wstring(url) + 
                  L" -> " + wstring_limit(data ? data : L"null") + 
                  L" -> " + wstring(contentType ? contentType : L"null") + 
                  L" -> " + wstring(headers ? headers : L"null") + 
                  L" -> " + boost::lexical_cast<wstring>(success) +
                  L" -> " + boost::lexical_cast<wstring>(error));
    
    CComObject<BindStatusCallback> *bindStatusCallback;
    hr = CComObject<BindStatusCallback>::CreateInstance(&bindStatusCallback);
    if (FAILED(hr)) {
        logger->error(L"NativeExtensions::getPage "
                      L"could not create BindStatusCallback instance"
                      L" -> " + logger->parse(hr));
        return hr;
    }

    hr = bindStatusCallback->Async(new HTTP(method, url, data, contentType, headers, success, error),
                                   (BindStatusCallback::ATL_PDATAAVAILABLE1)&HTTP::OnData,
                                   method,
                                   url,
                                   data,
                                   contentType,
                                   NULL, false);

    return hr;
}


/**
 * Method: NativeExtensions::notification
 *
 * @param icon
 * @param title
 * @param text
 */
STDMETHODIMP CNativeExtensions::notification(BSTR icon, BSTR title, BSTR text,
                                             BOOL *out_success)
{
    logger->debug(L"NativeExtensions::notification "
                  L" -> " + wstring(icon) + 
                  L" -> " + wstring(title) + 
                  L" -> " + wstring(text));

    // TODO if permission check fails -> *out_success = FALSE
    bfs::wpath path = _AtlModule.modulePath / L"forge\\graphics\\icon16.png";
    NotificationWindow::Notification(path.wstring(), title, text);

    *out_success = TRUE;
    return S_OK;
}

