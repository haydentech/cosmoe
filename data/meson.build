install_data('develop/makefile-engine', install_dir : '/usr/local/etc')

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