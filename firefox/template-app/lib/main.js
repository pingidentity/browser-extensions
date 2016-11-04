// Config
var config = {uuid: "${uuid}"};

// Jetpack libraries
var pageWorker = require("sdk/page-worker");
var pageMod = require("sdk/page-mod");
var data = require("sdk/self").data;
var request = require("sdk/request");
var notif = require("sdk/notifications");
// BE-265: For toolbar button is broken, changing to use latest
// SDK module of "sdk/ui/button/action" and removing "toolbarbutton"
// module.
var buttons = require('sdk/ui/button/action');

var ss = require("sdk/simple-storage");
if (!ss.storage || !ss.storage.prefs) {
	ss.storage.prefs = {}
}

var button;
var workers = [];
var background;

var addWorker = function (worker) {
	workers.push(worker);
}
var removeWorker = function (worker) {
	var index = workers.indexOf(worker);
	if (index != -1) {
		workers.splice(index, 1);
	}
}
var attachWorker = function (worker) {
	addWorker(worker);
	worker.on('detach', function () {
		removeWorker(this);
	});
	worker.on('message', handleNonPrivCall);
}
var handleNonPrivCall = function(msg) { 
	var self = this;
	var handleResult = function (status) {
		return function(content) {
			// create and send response message
			var reply = {callid: msg.callid, status: status, content: content};
			try {
				self.postMessage(reply);
			} catch (e) {
				// Probably a dead callback
				// TODO: GC somehow
			}
		}
	}
	call(msg.method, msg.params, self, handleResult('success'), handleResult('error'));
};

var nullFn = function () {};
var call = function(methodName, params, worker, success, error) {
	var strparams = "parameters could not be stringified";
	try {
		strparams = JSON.stringify(params);
	} catch (e) { }
	apiImpl.logging.log({
		message: "Received call to "+methodName+" with parameters: "+strparams,
		level: 10
	}, nullFn, nullFn);

	if (!success) {
		success = nullFn;
	} 
	if (!error) {
		error = nullFn;
	}
	
	try {
		// descend into API with dot-separated names
		var methodParts = methodName.split('.'),
		method = apiImpl;

		for (var i=0; i<methodParts.length; i++) {
			method = method[methodParts[i]];
		}

		if (typeof(method) !== 'function') {
			// tried to call non-existent API method
			error({message: methodName+' does not exist', type: "UNAVAILABLE"});
		}
		
		method.call(worker, params, function() {
			var strargs = "arguments could not be stringified";
			try {
				strargs = JSON.stringify(arguments);
			} catch (e) { }
			apiImpl.logging.log({
				message: 'Call to '+methodName+'('+strparams+') succeeded: '+strargs,
				level: 10
			}, nullFn, nullFn);
			success.apply(this, arguments);
		}, function() {
			var strargs = "arguments could not be stringified";
			try {
				strargs = JSON.stringify(arguments);
			} catch (e) { }
			apiImpl.logging.log({
				message: 'Call to '+methodName+'('+strparams+') failed: '+strargs,
				level: 30
			}, nullFn, nullFn);
			error.apply(this, arguments);
		});
	} catch (e) {
		error({message: 'Unknown error: '+e, type: "UNEXPECTED_FAILURE"})
	}
};

var apiImpl = {
	message: function (params) {
		var broadcast = {event: params.event, data: params.data, type: "message"};
		if (params.event == 'toFocussed') {
			broadcast.event = 'broadcast';
		}
		background.postMessage(broadcast);
		workers.forEach(function (worker) {
			if (params.event !== 'toFocussed' || worker.tab === require("sdk/tabs").activeTab) {
				try {
					worker.postMessage(broadcast);
				} catch (e) {
					// Probably just an about-to-be-removed worker, don't worry about it
				}
			}
		});
	},
	button: {
		setIcon: function (url, success, error) {		
			if (button) {
				button.icon = url;
				success()
			} else {
				error({message: 'Button does not exist', type: "UNAVAILABLE"});
			}
		},
		setURL: function (url, success, error) {
			if (button) {
				if (url && url.indexOf("http://") !== 0 && url.indexOf("https://") !== 0) {
					url = require("sdk/self").data.url('src'+(url.substring(0,1) == '/' ? '' : '/')+url);
				}
				button.url = url;
				success();
			} else {
				error({message: 'Button does not exist', type: "UNAVAILABLE"});
			}
		},
		setTitle: function (title, success, error) {
			if (button) {
				button.label = title;		
				success();
			} else {
				error({message: 'Button does not exist', type: "UNAVAILABLE"});
			}
		},
		setBadge: function (badgeText, success, error) {
			if (button) {
				button.badge = badgeText;			
				success();
			} else {
				error({message: 'Button does not exist', type: "UNAVAILABLE"});
			}

		},
		setBadgeBackgroundColor: function (badgeBGColor, success, error) {
			if (button) {
				button.badgeColor = badgeBGColor;
				success();
			} else {
				error({message: 'Button does not exist', type: "UNAVAILABLE"});
			}
		},
		onClicked: {
			addListener: function (params, callback, error) {
				if (button) {
					button.on(callback);
					button.url = '';
				} else {
					error({message: 'Button does not exist', type: "UNAVAILABLE"});
				}
			}
		}
	},
	logging: {
		log: function (params, success, error) {
			if (typeof console !== "undefined") {
				switch (params.level) {
					case 10:
						if (console.debug !== undefined && !(console.debug.toString && console.debug.toString().match('alert'))) {
							console.debug(params.message);
						}
						break;
					case 30:
						if (console.warn !== undefined && !(console.warn.toString && console.warn.toString().match('alert'))) {
							console.warn(params.message);
						}
						break;
					case 40:
					case 50:
						if (console.error !== undefined && !(console.error.toString && console.error.toString().match('alert'))) {
							console.error(params.message);
						}
						break;
					default:
					case 20:
						if (console.info !== undefined && !(console.info.toString && console.info.toString().match('alert'))) {
							console.info(params.message);
						}
						break;
				}
				success();
			}
		}
	},
	tools: {
		getURL: function (params, success, error) {
			name = params.name.toString();
			if (name.indexOf("http://") === 0 || name.indexOf("https://") === 0) {
				success(name);
			} else {
				success(data.url('src'+(name.substring(0,1) == '/' ? '' : '/')+name));
			}
		}
	},
	notification: {
		create: function (params, success, error) {
			require("sdk/notifications").notify({
				title: params.title,
				text: params.text
			});
			success();
		}
	},
	tabs: {
		open: function(params, success, error) {
			require('sdk/tabs').open({
				url: params.url,
				inBackground: params.keepFocus,
				onOpen: function () {
					success();
				}
			});
		},
		closeCurrent: function () {
			this.tab.close();
		},
		getCurrent: function(params, success, error)  {
			var tabs = require('sdk/tabs');
			var tab = tabs.activeTab;
			success({id: tab.id, url: tab.url});
		},
		/**
		 * OnTabSelectionChanged only calls the success callback once; if the background script wishes
		 * to get the next tab selection, it must call this function again.
		 *   TODO Figure out why success can only be called once below (subsquent calls to success appear to fail, 
		 *   so I am guessing that the framework is doing something to the function reference after it is called)
		 */
		onTabSelectionChanged: function(params, success, error) {
			apiImpl.logging.log({message: 'TABS [forge] onTabSelectionChanged register', level: 50 }, nullFn, nullFn);
			var tabs = require('sdk/tabs');
			tabs.on('activate', function (tab) {
				apiImpl.logging.log({message: 'TABS [forge] onTabSelectionChanged: ' + tab.id, level: 50 }, nullFn, nullFn);
				success({id: tab.id, url: tab.url});
			});
		},
		getAllTabs: function(params, success, error) {
			var tabs = require('sdk/tabs');
			var result = [];

			for (var tab = 0; tab < tabs.length; tab++) {
				result[result.length] = {
					id: tabs.__proto__[tab].id,
					url: tabs.__proto__[tab].url
				};
			}
			success(result);
		}
	},
	request: {
		ajax: function (params, success, error) {
		
			var complete = false;
			var timer = require('sdk/timers').setTimeout(function () {
				if (complete) return;
				complete = true;
				error && error({
					message: 'Request timed out',
					type: 'EXPECTED_FAILURE'
				});
			}, params.timeout ? params.timeout : 60000);
		
			var req = request.Request({
				url: params.url,
				onComplete: function (res) {
					require('sdk/timers').clearTimeout(timer);
					if (complete) return;
					complete = true;
				
					if (res.status >= 200 && res.status < 400) {
						success(res.text)
					} else {
						error({message: "HTTP error code received from server: "+res.status,
							statusCode: res.status,
							type: "EXPECTED_FAILURE"});
					}
				},
				// TODO: encode strings etc:
				content: params.data,
				headers: params.headers
			});

			req[params.type ? params.type.toLowerCase() : 'get']();
		}
	},
	prefs: {
		get: function(params, success, error) {
			success(ss.storage.prefs[params.key] === undefined ? null : ss.storage.prefs[params.key]);
		},
		set: function(params, success, error) {
			ss.storage.prefs[params.key] = params.value
			success();
		},
		keys: function(params, success, error) {
			success(Object.keys(ss.storage.prefs));
		},
		all: function(params, success, error) {
			success(ss.storage.prefs);
		},
		clear: function(params, success, error) {
			delete ss.storage.prefs[params.key];
			success();
		},
		clearAll: function(params, success, error) {
			success(ss.storage.prefs = {});
		},
		getSync: function(params) {
			return ss.storage.prefs[params.key] === undefined ? null : ss.storage.prefs[params.key];
		},
		setSync: function(params) {
			ss.storage.prefs[params.key] = params.value;
		},
		clearSync: function(params) {
			delete ss.storage.prefs[params.key];
		}
	},
	file: {
		string: function (file, success, error) {
			success(data.load(file.uri.substring(data.url('').length)));
		}
	}
};

// Load the extension
exports.main = function(options, callbacks) {
	// Button	
{% if "button" in plugins and "config" in plugins["button"] %}
	var dataUrl;
	{% if "default_popup" in plugins['button']['config'] %}
	dataUrl = data.url(${json.dumps(plugins['button']['config']['default_popup'])});
	{% end %}
 	button = buttons.ActionButton({
		id: config.uuid + "-button"
	{% if "default_title" in plugins["button"]["config"] %}
		, label: ${json.dumps(plugins['button']["config"]['default_title'])}
	{% end %}
	{% python
	def get_ba_icon(ba):
		if 'firefox' in ba.get('default_icons', {}):
			return ba['default_icons']['firefox']
		if 'default_icon' in ba:
			return ba['default_icon']
		return False
	%}
	{% if get_ba_icon(plugins['button']['config']) %}
		, icon: data.url(${json.dumps(get_ba_icon(plugins['button']['config']))})
	{% end %}
 		, onClick: function(state) {
			if (dataUrl) {
				// Create and destroy popups on demand (like Chrome)
				var panel = require("sdk/panel").Panel({
					contentURL: dataUrl,
					{% if "default_width" in plugins["button"]["config"] %} width: parseInt(${json.dumps(plugins['button']["config"]['default_width'])}), {% end %}
					{% if "default_height" in plugins["button"]["config"] %} height: parseInt(${json.dumps(plugins['button']["config"]['default_height'])}), {% end %}
					contentScriptFile: data.url("forge/api-firefox-proxy.js"),
					contentScriptWhen: "start",
					onMessage: handleNonPrivCall,
					onHide: function () {
						removeWorker(this);
						// Completely remove panel from DOM
						this.destroy();
					}
				});
				// Keep the panel in the list of workers for messaging
				addWorker(panel);
				panel.show({position: button});
			}
		}
 	});
{% end %}

	// Background page
	background = pageWorker.Page({
		contentURL: data.url('forge.html'),
		contentScriptFile: data.url("forge/api-firefox-proxy.js"),
		contentScriptWhen: "start",
		onMessage: handleNonPrivCall
	});
	
	// Convert between chrome match patterns and regular expressions
	var patternToRe = function (str) {
		if (str == '<all_urls>') {
			str = '*://*'
		}
		str = str.split('://');
		var scheme = str[0];
		var host, path;
		if (str[1].indexOf('/') === -1) {
			host = str[1];
			path = '';
		} else {
			host = str[1].substring(0, str[1].indexOf('/'));
			path = str[1].substring(str[1].indexOf('/'));
		}

		var re = '';

		// Scheme
		if (scheme == '*') {
			re += '(http|https|file|ftp)://';
		} else if (['http','https','file','ftp'].indexOf(scheme) !== -1) {
			re += scheme+'://';
		} else {
			// Invalid scheme
			return new RegExp('^$');
		}

		// Host
		if (host == '*') {
			re += '.*';
		} else if (host.indexOf('*.') === 0) {
			re += '(.+\.)?'+host.substring(2);
		} else {
			re += host;
		}
		
		// Path
		re += path.replace(/\*/g, '.*');
		
		return new RegExp(re);
	}
	var patternsToRe = function (arr) {
		if (arr.map) {
			return arr.map(patternToRe);
		} else {
			return patternToRe(arr);
		}
	}
	var strArrToDataUrl = function (arr) {
		if (arr.map) {
			return arr.map(data.url);
		} else {
			return data.url(arr);
		}
	};
	
{% if "activations" in plugins and "config" in plugins["activations"] and "activations" in plugins["activations"]["config"] and len(plugins["activations"]["config"]["activations"]) %}{% for activation in plugins["activations"]["config"]["activations"] %}
	pageMod.PageMod({
		include: patternsToRe(${json.dumps(activation.patterns)}),
		{% if activation.has_key("all_frames") and activation["all_frames"] is True %}
			contentScriptFile: strArrToDataUrl(${json.dumps(["forge/app_config.js", "forge/all.js"] + ["forge/jquery-2.2.4.js", "forge/jquery-ui-1.10.4.custom.js", "forge/jquery-noConflict.js"] + activation.scripts)}),
		{% end %} {% if not activation.has_key("all_frames") or activation["all_frames"] is False %}
			contentScriptFile: strArrToDataUrl(${json.dumps(["forge/app_config.js", "forge/all.js", "forge/disable-frames.js"] + ["forge/jquery-2.2.4.js", "forge/jquery-ui-1.10.4.custom.js", "forge/jquery-noConflict.js"] + activation.scripts)}),
		{% end %}
		{% if activation.has_key("run_at") %}
			contentScriptWhen: ${json.dumps(activation.run_at)},
		{% end %} {% if not activation.has_key("run_at") %}
			contentScriptWhen: 'end',
		{% end %}
		onAttach: function (worker) {
			attachWorker(worker);
			{% if activation.has_key("styles") %}
			var files = ${json.dumps(activation.styles)}
			for (var i in files) {
				files[i] = data.load(files[i]);
			}
			worker.postMessage({type: "css", files: files});
			{% end %}
			worker.port.emit('get_tab', {id: worker.tab.id, url: worker.tab.url});
		}
	});
{% end %}{% end %}

	// Local pages
	pageMod.PageMod({
		include: data.url('')+'*',
		contentScriptFile: data.url("forge/api-firefox-proxy.js"),
		contentScriptWhen: 'start',
		onAttach: attachWorker
	});
};

exports.onUnload = function (reason) {
	if ((reason === "uninstall") || (reason === "disable")) {
		ss.storage.prefs = {};	
	}

	if (button) {
		button.removeListener("click");
		button.destroy();
	}
};
