build_opts = [
    '-D_ISOC11_SOURCE', '-D_XOPEN_SOURCE=700', '-U__STRICT_ANSI__',
]

build_opts += cc.get_supported_arguments(
    # Warnings
    '-Wundef', '-Wmissing-prototypes', '-Wshadow', '-Wparentheses',
    '-Wpointer-arith', '-Wno-pointer-sign',

    # Warnings to treat as errors
    '-Werror=implicit-function-declaration',
)

# MSVC has <stdatomic.h> only with this flag (Visual Studio 17.5 and later)
if cc.get_argument_syntax() == 'msvc'
    build_opts += '/experimental:c11atomics'
endif

#fix for MinGW32
if host_machine.system() == 'windows' and cc.sizeof('void*') == 4
  build_opts += '-D__MINGW_USE_VC2005_COMPAT'
endif

static_build = get_option('default_library') == 'static'

# The macOS sub-channel reader needs cdio_get_device_fd()
libcdio_version = host_machine.system() == 'darwin' ? '>= 2.3.0' : '>= 2.0'

libcdio = dependency('libcdio', version: libcdio_version)

# MSVC has no pthreads: src/thread_compat.h gives cyanrip what it uses of
# them on Windows' own threads and locks, which need no library
if cc.get_argument_syntax() == 'msvc'
    threads = declare_dependency()
else
    threads = dependency('threads')
endif

# Required dependencies
dependencies = [
    # ffmpeg libs
    dependency('libavcodec', version: '>= 59.24.100'),
    dependency('libavformat', version: '>= 58.13.100'),
    dependency('libswresample', version: '>= 4.5.100'),
    dependency('libavfilter', version: '>= 7.16.100'),
    dependency('libavutil', version: '>= 57.25.100'),

    # other dependencies
    libcdio,
    dependency('libcdio_paranoia', version: '>= 10.2'),
    dependency('libmusicbrainz5', version: '>= 5.1', static: static_build),
    dependency('libcurl', version: '>=7.66.0'),

    # misc
    threads,
    cc.find_library('m', required : cc.get_argument_syntax() != 'msvc'),
]

# Base files
sources = [
    'cyanrip_encode.c',
    'cyanrip_log.c',
    'cyanrip_main.c',
    'naming.c',
    'fun512.c',
    'utils.c',

    'fifo_frame.c',
    'fifo_packet.c',

    'discid.c',
    'musicbrainz.c',
    'qrcode.c',
    'coverart.c',
    'accurip.c',
    'acoustid.c',
    'json.c',

    'cue_writer.c',
    'subq_read.c',

    'pregap.c',

    # Version
    vcs_tag(command: ['git', 'rev-parse', '--short', 'HEAD'],
                      input: 'version.c.in', output: 'version.c',
                      fallback: 'release')
]

# Newer libcdio reads sub-channel data itself, on every platform and from disc images
if cc.has_function('cdio_read_audio_sectors_sub', prefix: '#include <cdio/cdio.h>',
                   dependencies: libcdio)
    sources += 'subq_read_cdio.c'
elif host_machine.system() == 'darwin'
    sources += 'subq_read_macos.c'
else
    sources += 'subq_read_mmc.c'
endif

# Optional, for printing MusicBrainz submission URLs as QR codes
qrencode = dependency('libqrencode', version: '>= 4.0', required: false,
                      static: static_build)
if qrencode.found()
    conf.set('HAVE_QRENCODE', 1)
    dependencies += qrencode
else
    message('libqrencode not found, MusicBrainz URLs won\'t be printed as QR codes')
endif

# Optional, for AcoustID fingerprinting
chromaprint = dependency('libchromaprint', version: '>= 1.4', required: false,
                         static: static_build)
if chromaprint.found()
    conf.set('HAVE_CHROMAPRINT', 1)
    dependencies += chromaprint
else
    message('libchromaprint not found, tracks won\'t be fingerprinted for AcoustID')
endif

# Check for wmain support (Windows). MinGW needs -municode for it, MSVC
# finds wmain() by itself.
municode = cc.get_argument_syntax() == 'msvc' ? [] : ['-municode']
if host_machine.system() == 'windows' and cc.links('int wmain() { return 0; }', args: municode)
     conf.set('HAVE_WMAIN', 1)
     build_opts += municode
endif

add_global_arguments(build_opts, language: 'c')

cyanrip_exe = executable('cyanrip',
    install: true,
    sources: sources,
    dependencies: dependencies,
)
