module("forge.document");

asyncTest("Check document.location", 1, function() {
	forge.document.location(function(location) {
		var expect = {};
		if (forge.is.chrome()) {
			expect.protocol = "chrome-extension:";
		} else if (forge.is.firefox()) {
			expect.protocol = "resource:";
		} else {
			expect.protocol = "file:";
		}
		equal(location.protocol, expect.protocol, "Check location.protocol");
		start();
	});
});

