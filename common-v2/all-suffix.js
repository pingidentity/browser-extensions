
// END concatenate_files
})();

// BE-2268
// Restore [define] to its original
if (typeof tmpDefine === "function" && tmpDefine.amd) {
    define = tmpDefine;
}