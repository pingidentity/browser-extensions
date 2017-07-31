forge['is'] = {
	/**
	 * @return {boolean}
	 */
	'mobile': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'desktop': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'android': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'ios': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'chrome': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'firefox': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'safari': function() {
		return false;
	},
	/**
	 * @return {boolean}
	 */
	'ie': function() {
		return false;
	},
    /**
     * @return {boolean}
     */
    'edge': function() {
        if (window !== undefined && window.navigator !== undefined) {
            var ua = window.navigator.userAgent;
            if (ua !== undefined) {
                var edge = ua.indexOf('Edge/');
                if (edge > 0) {
                    return true;
                }

                return false;
            }
        }
    },
	/**
	 * @return {boolean}
	 */
	'web': function() {
		return false;
	},
	'orientation': {
		'portrait': function () {
			return false;
		},
		'landscape': function () {
			return false;
		}
	},
	'connection': {
		'connected': function () {
			return true;
		},
		'wifi': function () {
			return true;
		}
	}
};

