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
    // START concatenate_files
