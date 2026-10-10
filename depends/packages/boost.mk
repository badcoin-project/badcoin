package=boost
$(package)_version=1_64_0
$(package)_download_path=https://dl.bintray.com/boostorg/release/1.64.0/source/
$(package)_file_name=$(package)_$($(package)_version).tar.bz2
$(package)_sha256_hash=7bcc5caace97baa948931d712ea5f37038dbb1c5d89b43ad4def4ed7cb683332

define $(package)_set_vars
$(package)_config_opts_release=variant=release
$(package)_config_opts_debug=variant=debug
$(package)_config_opts=--layout=tagged --build-type=complete --user-config=user-config.jam
$(package)_config_opts+=threading=multi link=static -sNO_BZIP2=1 -sNO_ZLIB=1
$(package)_config_opts_linux=threadapi=pthread runtime-link=shared
$(package)_config_opts_darwin=--toolset=darwin-4.2.1 runtime-link=shared
$(package)_config_opts_mingw32=binary-format=pe target-os=windows threadapi=win32 runtime-link=static
$(package)_config_opts_x86_64_mingw32=address-model=64
$(package)_config_opts_i686_mingw32=address-model=32
$(package)_config_opts_i686_linux=address-model=32 architecture=x86
$(package)_toolset_$(host_os)=gcc
$(package)_archiver_$(host_os)=$($(package)_ar)
$(package)_toolset_darwin=darwin
$(package)_archiver_darwin=$($(package)_libtool)
$(package)_config_libraries=chrono,filesystem,program_options,system,thread,test
$(package)_cxxflags=-std=c++11 -fvisibility=hidden
$(package)_cxxflags_linux=-fPIC
endef

define $(package)_preprocess_cmds
  echo "using $(boost_toolset_$(host_os)) : : $($(package)_cxx) : <cxxflags>\"$($(package)_cxxflags) $($(package)_cppflags)\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$(boost_archiver_$(host_os))\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam && if test "$(host_os)" = darwin; then perl -0pi -e 's{darwin\)(\s+)BOOST_JAM_CC=cc}{darwin)\x0A    BOOST_JAM_CC="cc -Wno-error=implicit-function-declaration"}' tools/build/src/engine/build.sh && perl -0pi -e 's{toolset darwin cc\s+:\s+"-o "\s+:\s+-D\s*:}{toolset darwin cc :  "-o " : -D\x0A    : -Wno-error=implicit-function-declaration}' tools/build/src/engine/build.jam && perl -0pi -e 's{\s+-fcoalesce-templates}{}g' tools/build/src/tools/darwin.jam && perl -0pi -e 's{(#if \(_LIBCPP_VERSION < 4000\) \|\| \(__cplusplus <= 201402L\)\n#  define BOOST_NO_CXX17_STD_APPLY\n#endif)}{$$1\n#if (_LIBCPP_VERSION > 4000) && (__cplusplus > 201402L) && !defined(_LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR)\n#  define BOOST_NO_AUTO_PTR\n#endif\n#if (_LIBCPP_VERSION > 4000) && (__cplusplus > 201402L) && !defined(_LIBCPP_ENABLE_CXX17_REMOVED_BINDERS)\n#  define BOOST_NO_CXX98_BINDERS\n#endif}' boost/config/stdlib/libcpp.hpp && grep -F '#  define BOOST_NO_AUTO_PTR' boost/config/stdlib/libcpp.hpp && grep -F '#  define BOOST_NO_CXX98_BINDERS' boost/config/stdlib/libcpp.hpp && perl -0pi -e 's{#if defined\(_HAS_AUTO_PTR_ETC\) && !_HAS_AUTO_PTR_ETC}{#if (defined(_HAS_AUTO_PTR_ETC) \&\& !_HAS_AUTO_PTR_ETC) || defined(BOOST_NO_CXX98_BINDERS)}' boost/functional/hash/hash.hpp && grep -F '#if (defined(_HAS_AUTO_PTR_ETC) && !_HAS_AUTO_PTR_ETC) || defined(BOOST_NO_CXX98_BINDERS)' boost/functional/hash/hash.hpp; if test "$(host_arch)" = arm; then perl -0pi -e 's{options = -arch arm ;}{options = -arch arm64 ;}' tools/build/src/tools/darwin.jam && grep -F 'options = -arch arm64 ;' tools/build/src/tools/darwin.jam; fi; fi
endef

define $(package)_config_cmds
  ./bootstrap.sh --without-icu --with-libraries=$(boost_config_libraries)
endef

define $(package)_build_cmds
  ./b2 -d2 -j2 -d1 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) stage
endef

define $(package)_stage_cmds
  ./b2 -d0 -j4 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) install
endef
