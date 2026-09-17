install_subdir('os', install_dir: dir_include)
install_subdir('private', install_dir: dir_include)
install_subdir('config', install_dir: dir_include)
install_subdir('posix', install_dir: dir_include)

# Install libs directory
install_subdir('libs/agg', install_dir: dir_include / 'libs')

# Install wayland-stubs.h to os/interface for compatibility
install_headers('libs/wayland/stubs/wayland-stubs.h', 
                subdir: 'os/interface')