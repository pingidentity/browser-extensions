from os import path

def prepare_config():
	return [
		{'do': {'preprocess_config': ()}},
		{'when': {'platform_is': 'safari'}, 'do': {'need_https_access': ()}},
	]

def copy_platform_source():
	return [
		{'do': {'addon_source': 'common-v2'}},
		{'do': {'addon_source': 'plugin/schema'}},
		{'when': {'platform_is': 'firefox'}, 'do': {'addon_source': 'firefox/template-app'}},
		{'when': {'platform_is': 'chrome'}, 'do': {'addon_source': 'chrome'}},
		{'when': {'platform_is': 'safari'}, 'do': {'addon_source': 'forge.safariextension'}},
		{'when': {'platform_is': 'ie'}, 'do': {'addon_source': 'ie'}},
	]

def override_plugins():
	return [
		{'do': {'prepare_plugin_override': ()}},
	]

def copy_customer_source():
	return [
		{'do': {'user_source': 'src'}},
	]

def copy_common_files():
	return [
		{'when': {'platform_is': 'chrome'}, 'do': {'copy_files': {
			'from': 'common-v2/forge', 'to': 'chrome/forge'
		}}},
		{'when': {'platform_is': 'firefox'}, 'do': {'copy_files': {
			'from': 'common-v2/forge', 'to': 'firefox/template-app/data/forge'
		}}},
		{'when': {'platform_is': 'safari'}, 'do': {'copy_files': {
			'from': 'common-v2/forge', 'to': 'forge.safariextension/forge'
		}}},
		{'when': {'platform_is': 'ie'}, 'do': {'copy_files': {
			'from': 'common-v2/forge', 'to': 'ie/forge'
		}}},
	]

def sensible_default_for_toolbar():
	return [
		{'do': {'fallback_to_default_toolbar_icon': ()}},
	]

def pre_create_all_js():
	return [
		# start file
		{'do': {'add_to_all_js': 'common-v2/all-prefix.js'}},

		{'when': {'platform_is': 'ie'}, 'do': {'add_to_all_js': 'common-v2/json2.js'}},

		# jquery
		{'do': {'add_to_all_js': 'common-v2/jquery-2.2.4.js'}},
		{'do': {'add_to_all_js': 'common-v2/jquery-ui-1.10.4.custom.js'}},
		{'do': {'add_to_all_js': 'common-v2/jquery-noConflict.js'}},
		# {'do': {'add_to_all_js': 'common-v2/api-jquery.js'}},
		
		# start api
		{'do': {'add_to_all_js': 'common-v2/api-prefix.js'}},
		{'do': {'add_to_all_js': 'common-v2/config.js'}},
		{'do': {'add_to_all_js': 'common-v2/api.js'}},
	]

def post_create_all_js():
	return [
		# common api
		{'do': {'add_to_all_js': 'common-v2/modules/is/common.js'}},
		{'do': {'add_to_all_js': 'common-v2/modules/logging/default.js'}},
		{'do': {'add_to_all_js': 'common-v2/modules/internal/default.js'}},
		{'do': {'add_to_all_js': 'common-v2/modules/event/common.js'}},
		{'do': {'add_to_all_js': 'common-v2/reload.js'}},
		{'do': {'add_to_all_js': 'common-v2/tools.js'}},

		# specific browser api
		{'when': {'platform_is': 'chrome'}, 'do': {'concatenate_files': {
			'in': ('chrome/assets_forge/api-chrome.js',),
			'out': 'chrome/forge/all.js'
		}}},
		{'when': {'platform_is': 'chrome'}, 'do': {'concatenate_files': {
			'in': ('common-v2/api-priv.js', 'chrome/assets_forge/api-priv-chrome.js',),
			'out': 'chrome/forge/all-priv.js'
		}}},
		{'when': {'platform_is': 'safari'}, 'do': {'concatenate_files': {
			'in': ('forge.safariextension/assets_forge/api-safari.js',),
			'out': 'forge.safariextension/forge/all.js'
		}}},
		{'when': {'platform_is': 'safari'}, 'do': {'concatenate_files': {
			'in': ('common-v2/api-priv.js', 'forge.safariextension/assets_forge/api-priv-safari.js'),
			'out': 'forge.safariextension/forge/all-priv.js'
		}}},
		{'when': {'platform_is': 'ie'}, 'do': {'concatenate_files': {
			'in': ('ie/assets_forge/api-proxy.js', 'ie/assets_forge/api-ie.js',),
			'out': 'ie/forge/all.js'
		}}},
		{'when': {'platform_is': 'ie'}, 'do': {'concatenate_files': {
			'in': ('common-v2/api-priv.js', 'ie/assets_forge/api-proxy.js', 'ie/assets_forge/api-priv-ie.js',),
			'out': 'ie/forge/all-priv.js'
		}}},
		{'when': {'platform_is': 'firefox'}, 'do': {'add_to_all_js': 'firefox/template-app/data/assets_forge/api-firefox.js'}},

		# close api
		{'do': {'add_to_all_js': 'common-v2/api-expose.js'}},
		{'do': {'add_to_all_js': 'common-v2/api-suffix.js'}},

		# close file
		{'do': {'add_to_all_js': 'common-v2/all-suffix.js'}},

		# specific for Firefox
		{'when': {'platform_is': 'firefox'}, 'do': {'rename_files': {
			'from': 'firefox/template-app/data/assets_forge/api-firefox-bg.js',
			'to': 'firefox/template-app/data/forge/api-firefox-bg.js'
		}}},
		{'when': {'platform_is': 'firefox'}, 'do': {'rename_files': {
			'from': 'firefox/template-app/data/assets_forge/api-firefox-proxy.js',
			'to': 'firefox/template-app/data/forge/api-firefox-proxy.js'
		}}},
	]
	
def copy_def_prefs_loader():
	return [
		{'when': {'platform_is': 'firefox'}, 'do': {'copy_files': {
			'from': 'firefox/template-app/module', 'to': 'firefox/template-app/data/forge/module'
		}}},
	]

def remove_assets_forge():
	return [
		{'when': {'platform_is': 'chrome'}, 'do': {'remove_files': 'chrome/assets_forge'}},
		{'when': {'platform_is': 'firefox'}, 'do': {'remove_files': 'firefox/template-app/data/assets_forge'}},
		{'when': {'platform_is': 'safari'}, 'do': {'remove_files': 'forge.safariextension/assets_forge'}},
		{'when': {'platform_is': 'ie'}, 'do': {'remove_files': 'ie/assets_forge'}},
	]

def platform_specific_templating(build):
	'Perform any platform specific templating'
	
	return [
		{'when': {'platform_is': 'chrome'}, 'do': {'template_files': (
			'chrome/forge.html',
			'chrome/manifest.json'
		)}},
		
		{'when': {'platform_is': 'firefox'}, 'do': {'template_files': (
			'firefox/template-app/package.json',
			'firefox/template-app/lib/main.js',	
			'firefox/template-app/data/forge/module/defaultPreferencesLoader.jsm',		
			'firefox/template-app/data/forge.html',
		)}},
		
		{'when': {'platform_is': 'safari'}, 'do': {'template_files': (
			'forge.safariextension/forge.html',
			'forge.safariextension/Info.plist'
		)}},
		
		{'when': {'platform_is': 'ie'}, 'do': {'template_files': (
			'ie/forge.html',
			'ie/manifest.json',
			'ie/dist/setup-x86.nsi',
			'ie/dist/setup-x64.nsi',
			'ie/msvc/BHO.vcxproj'
		)}},
	]

def minification():
	return [		
		{'when': {'platform_is': 'chrome', 'is_external': ()}, 'do': {'minify_in_place': (
			'chrome/forge/all.js',
			'chrome/forge/all-priv.js',
		)}},
		
		{'when': {'platform_is': 'firefox', 'is_external': ()}, 'do': {'minify_in_place': (
			'firefox/template-app/data/forge/all.js',
			'firefox/template-app/data/forge/api-firefox-bg.js',
			'firefox/template-app/data/forge/api-firefox-proxy.js',
			'firefox/template-app/lib/main.js',
			'firefox/template-app/data/forge/module/defaultPreferencesLoader.jsm',			
		)}},

		{'when': {'platform_is': 'safari', 'is_external': ()}, 'do': {'minify_in_place': (
			'forge.safariextension/forge/all.js',
			'forge.safariextension/forge/all-priv.js',
		)}},
		
		{'when': {'platform_is': 'ie', 'is_external': ()}, 'do': {'minify_in_place': (
			'ie/forge/all.js',
			'ie/forge/all-priv.js',
		)}},
	]


def add_plugins():
	'Run any platform specific steps required to include native plugins'
	return [
	]

def platform_specific_build():
	'Run any platform specific build steps'
	return [
		{'when': {'platform_is': 'firefox'}, 'do': {'cfx_build': 'firefox/template-app'}},
		{'when': {'platform_is': 'firefox'}, 'do': {'extract_files': {
			'from': 'firefox/template-app/f.xpi',
			'to': 'firefox/template-app/output'
		}}},
        {'when': {'platform_is': 'firefox'}, 'do': {'copy_package_file': {
			'from': 'firefox/template-app/package.json',
			'to': 'firefox/template-app/output/resources/f/package.json'
		}}},
		{'when': {'platform_is': 'ie'}, 'do': {'remove_files': (
			'ie/build/Win32/Debug',
			'ie/build/x64/Debug',
			'ie/dist/build.bat',
			'ie/dist/certs.dev',
			'ie/dist/regenerate.bat',
			'ie/doc',
			'ie/external',
			'ie/msvc',
			'ie/source',
		)}},
	]

def handle_template_output():
	return [
		{'do': {'remove_files': 'common-v2'}},
		{'when': {'platform_is': 'chrome'}, 'do': {'rename_files': {
			"from": "chrome",
			"to": "development/chrome"
		}}},
		{'when': {'platform_is': 'firefox'}, 'do': {'rename_files': {
			"from": "firefox/template-app/output",
			"to": "development/firefox"
		}}},
		{'when': {'platform_is': 'safari'}, 'do': {'rename_files': {
			"from": "forge.safariextension",
			"to": "development/forge.safariextension"
		}}},
		{'when': {'platform_is': 'ie'}, 'do': {'rename_files': {
			"from": "ie",
			"to": "development/ie"
		}}},
	]

def copy_lib_files_to_template(source_dir):
	return [
		{'do': {'copy_files': {
			'from': path.join(source_dir, 'generate', 'lib'),
			'to': path.join('.template', 'lib')
		}}},
	]

def handle_output():
	return [
		{'do': {'move_output': 'development'}},
		{'when': {'do_package': ()}, 'do': {'move_output': 'release'}},
		{'do': {'remember_build_output_location': ()}},
	]

def handle_debug_output():
	return [
	]
