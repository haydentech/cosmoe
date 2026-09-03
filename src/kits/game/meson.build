fs = import('fs')

# Miniaudio is the only supported Game Kit backend on Cosmoe.
if not fs.exists(meson.project_source_root() / 'third_party/miniaudio/miniaudio.h')
    error('Game Kit requires vendored miniaudio header at third_party/miniaudio/miniaudio.h')
endif

srcs_game = [
    'DirectWindow.cpp',
    'GameAudioBackendMiniaudio.cpp',
    'GameSoundDevice.cpp',
    'GameSound.cpp',
    'SimpleGameSound.cpp',
    'StreamingGameSound.cpp',
    'FileGameSound.cpp',
    'PushGameSound.cpp',
]

message('Game Kit sources enabled: backend=miniaudio')

srcs_game = files(srcs_game)