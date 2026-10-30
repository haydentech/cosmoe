makefile_engine_conf = configuration_data()
makefile_engine_conf.set('COSMOE_INSTALL_ROOT_DEFAULT', get_option('prefix'))

makefile_engine_installed = configure_file(
	input : 'develop/makefile-engine',
	output : 'makefile-engine',
	configuration : makefile_engine_conf,
)

install_data(
	makefile_engine_installed,
	'develop/macos-bundle.sh',
	install_dir : get_option('sysconfdir'),
)

# These scripts run on the machine doing the install, so they get the
# build-machine tools rather than the target binaries.  They need absolute
# paths: install scripts do not run from the build directory.
meson.add_install_script(
	'sh',
	meson.project_source_root() / 'data/install_mime_db.sh',
	meson.project_source_root() / 'src/data/mime_db',
	rc_app.full_path(),
	mime_db_builder_tool.full_path(),
	meson.project_build_root(),
	dir_cosmoe_data / 'mime_db',
)

# Convert shipped .catkeys into runtime .catalog files at install time.
# They are installed under <datadir>/cosmoe/locale/catalogs/<signature>/<lang>.catalog
meson.add_install_script(
	'sh',
	meson.project_source_root() / 'data/install_catalogs.sh',
	meson.project_source_root() / 'data/catalogs',
	linkcatkeys_tool.full_path(),
	dir_cosmoe_data / 'locale' / 'catalogs',
)

# Required for our Wayland windowframe
install_data('artwork/icon_window.png', install_dir : dir_cosmoe_data)
install_data('artwork/sign_close.png', install_dir : dir_cosmoe_data)
install_data('artwork/sign_maximize.png', install_dir : dir_cosmoe_data)
install_data('artwork/sign_minimize.png', install_dir : dir_cosmoe_data)

install_data('artwork/cosmoe-logo.png', install_dir : dir_cosmoe_data / 'artwork')