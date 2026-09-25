# Font installation
if is_darwin
    # Get list of all TrueType fonts
    font_files = run_command('sh', '-c', 'cd ttfonts && ls *.ttf', check: true).stdout().strip().split('\n')
    
    # Install fonts to user's Library/Fonts directory
    # Use install script to handle ~/Library/Fonts correctly (even when using sudo)
    meson.add_install_script('sh', '-c', '''
        # Determine the actual user (not root when using sudo)
        if [ -n "$SUDO_USER" ]; then
            ACTUAL_USER="$SUDO_USER"
            USER_HOME=$(eval echo ~$SUDO_USER)
        else
            ACTUAL_USER="$USER"
            USER_HOME="$HOME"
        fi
        
        FONT_DIR="${USER_HOME}/Library/Fonts/Cosmoe"
        echo "Installing Cosmoe fonts to ${FONT_DIR}..."
        
        mkdir -p "${FONT_DIR}"
        
        # Copy all .ttf files
        cp "''' + meson.current_source_dir() + '''/ttfonts/"*.ttf "${FONT_DIR}/"
        
        # Set ownership to actual user
        if [ -n "$SUDO_USER" ]; then
            chown -R "$ACTUAL_USER" "${FONT_DIR}"
        fi
        
        echo "Installed ''' + font_files.length().to_string() + ''' fonts to ${FONT_DIR}"
    ''')
endif

# Linux fontconfig scans /usr/share/fonts. Install Cosmoe's bundled fonts
if is_linux
    install_subdir(
        'ttfonts',
        install_dir: join_paths(get_option('datadir'), 'fonts', 'ttf', 'cosmoe'),
        strip_directory: true,
    )
endif
