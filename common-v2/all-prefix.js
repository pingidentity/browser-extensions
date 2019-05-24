// BE-2268
// Prevent jQueryBE to be register in AMD
// which cause conflict when client site use RequireJS
if (typeof define === "function" && define.amd) {
    window.tmpDefine = define;
    define = undefined;
}

(function () {
    function inIframe() {
        try {
            return window.self !== window.top;
        } catch (e) {
            return true;
        }
    }

    if (typeof document.documentMode === 'number') {
        if (document.documentMode <= 8) {
            if (typeof console === 'object') {
                if (typeof console.log === 'function') {
                    console.log('[PingOne] IE8 and below are not supported!');
                }
            }
            return;
        } else {
            // BE-2553
            // When un-checking the setting, BHO can't re-init objects: window.extensions, window.messaging & window.accessible
            // => request the user to restart their browser.
            if (typeof window.extensions !== 'object' && typeof window.messaging !== 'object' && typeof window.accessible !== 'object') {

                // There's a case when user open a popup/window from their site, BHO fail when attach forge (Access's denied when GetDispID)
                // and it's lead to window.extensions, window.messaging and window.accessible is undefined.
                // => This alert will be prompted and annoy user since restart doesn't resolve problem.
                // User will see this alert again when go back to this site after restart.
                // So I rem this code as a workaround and will try to find another solution for this issue later

                // if (inIframe()) {
                //     // do nothing
                // } else {
                //     alert('[PingOne] Please restart your browser!');
                // }

                return;
            }
        }
    }
    // START concatenate_files
