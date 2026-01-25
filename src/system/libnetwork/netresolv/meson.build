# netresolv - DNS resolver library (object file to be linked into libnetwork)

netresolv_inc_dirs = include_directories(
    'include',
    '.',
)

# NetResolv sources from different subdirectories
netresolv_sources = files(
    # dst
    'dst/dst_api.c',
    'dst/hmac_link.c',
    'dst/md5_dgst.c',
    'dst/support.c',

    # inet
    'inet/inet_cidr_ntop.c',
    'inet/inet_cidr_pton.c',
    'inet/inet_net_ntop.c',
    'inet/inet_net_pton.c',
    'inet/inet_neta.c',
    'inet/nsap_addr.c',

    # net (irs functions)
    'net/base64.c',
    'net/getaddrinfo.c',
    'net/gethnamaddr.c',
    'net/getnameinfo.c',
    'net/getnetent.c',
    'net/getnetnamadr.c',
    'net/getprotobyname.c',
    'net/getprotobyname_r.c',
    'net/getprotobynumber.c',
    'net/getprotobynumber_r.c',
    'net/getprotoent.c',
    'net/getprotoent_r.c',
    'net/getservbyname.c',
    'net/getservbyname_r.c',
    'net/getservbyport.c',
    'net/getservbyport_r.c',
    'net/getservent.c',
    'net/getservent_r.c',
    'net/hesiod.c',
    'net/nsdispatch.c',
    'net/sethostent.c',
    'net/vars6.c',

    # isc
    'isc/assertions.c',
    'isc/ev_streams.c',
    'isc/ev_timers.c',

    # nameser
    'nameser/ns_name.c',
    'nameser/ns_netint.c',
    'nameser/ns_parse.c',
    'nameser/ns_print.c',
    'nameser/ns_samedomain.c',
    'nameser/ns_ttl.c',

    # resolv
    'resolv/herror.c',
    'resolv/h_errno.c',
    'resolv/mtctxres.c',
    'resolv/res_comp.c',
    'resolv/res_data.c',
    'resolv/res_debug.c',
    'resolv/res_init.c',
    'resolv/res_mkquery.c',
    'resolv/res_query.c',
    'resolv/res_send.c',
    'resolv/res_state.c',
)

# Flex and Bison generated files
flex = find_program('flex', required: false)
bison = find_program('bison', required: false)

if flex.found() and bison.found()
    nslexer_c = custom_target('nslexer',
        input: 'net/nslexer.l',
        output: 'nslexer.c',
        command: [flex, '-o', '@OUTPUT@', '@INPUT@']
    )
    
    nsparser_gen = custom_target('nsparser',
        input: 'net/nsparser.y',
        output: ['nsparser.c', 'nsparser.h'],
        command: [bison, '-d', '-o', '@OUTPUT0@', '@INPUT@']
    )
    
    netresolv_sources += [nslexer_c, nsparser_gen]
else
    warning('flex or bison not found, netresolv may have incomplete build')
endif

# Compiler flags: _LIBC _REENTRANT=1 USE_MD5=1 INET6 lint LINT
netresolv_c_args = [
    '-D_LIBC',
    '-D_REENTRANT=1',
    '-DUSE_MD5=1',
    '-DINET6',
    '-Dlint',
    '-DLINT',
    '-Wno-sign-compare',
    '-Wno-unused-parameter',
    '-Wno-unused-variable',
]

# Build as object library to be linked into libnetwork.so
netresolv_lib = static_library(
    'netresolv',
    netresolv_sources,
    include_directories: [common_inc, netresolv_inc_dirs],
    c_args: netresolv_c_args,
    pic: true,  # Position independent code for shared library
)
