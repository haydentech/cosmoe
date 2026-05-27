makefile_engine_conf = configuration_data()
makefile_engine_conf.set('COSMOE_INSTALL_ROOT_DEFAULT', get_option('prefix'))

makefile_engine_installed = configure_file(
	input : 'develop/makefile-engine',
	output : 'makefile-engine',
	configuration : makefile_engine_conf,
)

install_data(makefile_engine_installed, install_dir : get_option('sysconfdir'))

meson.add_install_script(
	'sh',
	meson.project_source_root() / 'data/install_mime_db.sh',
	meson.project_source_root() / 'src/data/mime_db',
	meson.project_build_root() / 'rc',
	meson.project_build_root() / 'mime_db_builder',
	meson.project_build_root(),
)

# Convert shipped .catkeys into runtime .catalog files at install time.
# They are installed under /usr/local/etc/cosmoe/locale/catalogs/<signature>/<lang>.catalog
meson.add_install_script(
	'sh',
	meson.project_source_root() / 'data/install_catalogs.sh',
	meson.project_source_root() / 'data/catalogs',
	meson.project_build_root() / 'linkcatkeys',
)

# Required for our Wayland windowframe
install_data('artwork/icon_window.png', install_dir : '/usr/local/share/cosmoe')
install_data('artwork/sign_close.png', install_dir : '/usr/local/share/cosmoe')
install_data('artwork/sign_maximize.png', install_dir : '/usr/local/share/cosmoe')
install_data('artwork/sign_minimize.png', install_dir : '/usr/local/share/cosmoe')