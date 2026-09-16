install_subdir('os', install_dir: '/usr/local/include')
install_subdir('private', install_dir: '/usr/local/include')
install_subdir('config', install_dir: '/usr/local/include')
install_subdir('posix', install_dir: '/usr/local/include')

# Install libs directory
install_subdir('libs/agg', install_dir: '/usr/local/include/libs')

# Install wayland-stubs.h to os/interface for compatibility
install_headers('libs/wayland/stubs/wayland-stubs.h', 
                subdir: 'os/interface')