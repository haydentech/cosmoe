fs = import('fs')

dep_expat = dependency('expat', required: false)

if dep_expat.found()
  fs.copyfile('lion.svg', 'lion.svg')
  fs.copyfile('butterfly.svg', 'butterfly.svg')

  executable(
    'PictureTest',
    ['PictureTest.cpp', 'SVGViewView.cpp'],
    include_directories: common_inc,
    dependencies: [dep_libroot, dep_expat],
    link_with: lib_libbe,
    install: false
  )
endif