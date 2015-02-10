#include "stdafx.h"
#include "HTTP.h"

using std::string;

/**
 * HTTP::OnData
 */
void HTTP::OnData(BindStatusCallback *caller, BYTE *bytes, DWORD size)  
{
    // buffer data
    if (bytes) {
        /*logger->debug(L"HTTP::OnData"
                      L" -> " + this->url + 
                      L" -> " + boost::lexical_cast<wstring>(size) + L" bytes");*/
        this->size += size;
        this->bytes->insert(this->bytes->end(), bytes, bytes + size);
        return;
    }

    // TODO TODO TODO this assumes that the charset for the content-type is UTF-8. The content-type
    // should probably be extracted from the response headers. But, that approach is quite involved
    // since the response headers are only exposed as the raw response, which would leave parsing
    // up to the extension (a non-trival task).
    //
    // However, this is better than the previous version of this code that assumed an ASCII
    // encoding of the data from the server. So, at the very least, this code is backwards
    // compatible with the previous approach.
    string response_utf8(this->bytes->begin(), this->bytes->end());

    wchar_t *wide_response = NULL;
    DWORD convert_result = widestring_from_utf8(response_utf8.c_str(), int(response_utf8.size()),
            &wide_response);
    if (wide_response == NULL || convert_result != ERROR_SUCCESS) {
        wstring err_msg = L"{ \"error\" : \"failed to convert response to UTF-16\" }";
        logger->error(L"HTTP::OnData: failed to convert response to UTF-16" + 
                boost::lexical_cast<wstring>(convert_result));
        HRESULT hr = CComDispatchDriver(this->error).Invoke1((DISPID)0,
                &CComVariant(err_msg.c_str()));
        if (FAILED(hr)) {
            logger->error(L"HTTP::OnData Failed to invoke error callback" +
                    logger->parse(hr));
            return;
        }
    }

    // transfer complete
    wstring response(wide_response);
    delete[] wide_response;
    /*
    logger->debug(L"HTTP::OnData"
                  L" -> " + this->url + 
                  L" -> " + boost::lexical_cast<wstring>(this->size) + L" bytes " +
                  L" -> |" + wstring_limit(response) + L"|");
                  */
    
    // invoke appropriate callback
    if (this->callback) { 
        //logger->debug(L"HTTP::OnData -> IDispatch*");
        HRESULT hr = CComDispatchDriver(this->callback)
            .Invoke1((DISPID)0, &CComVariant(response.c_str()));
        if (FAILED(hr)) {
            logger->error(L"HTTP::OnData Failed to invoke event OnComplete"
                          L" -> " + logger->parse(hr));
        }
    } else if (this->callback_std) { 
        logger->debug(L"HTTP::OnData -> callback");
        this->callback_std(response);
    } else {
        logger->debug(L"HTTP::OnData -> no callback registered");
    }
    //logger->debug(L"HTTP::OnData -> done");

    // TODO error handling
    
    // cleanup
    //delete this; TODO fix this leak sans abominations
}


