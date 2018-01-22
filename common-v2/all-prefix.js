// BE-2268
// Prevent jQueryBE to be register in AMD
// which cause conflict when client site use RequireJS
if (typeof define === "function" && define.amd) {
    window.tmpDefine = define;
    define = undefined;
}

(function () {
    if (document.all && !document.addEventListener) {
        console.log('[PingOne] IE8 and below are not supported!');
        return;
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
                alert('[PingOne] Please restart your browser!');
                return;
            }
        }
    }
    // START concatenate_files
