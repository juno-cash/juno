package=native_cctools
# Updated to OSXCross versions for TAPI v4 support (required for macOS ARM64/Apple Silicon)
$(package)_version=e64ad89e63343749d5207a0a6507fe30d91dc2d8
$(package)_download_path=https://github.com/tpoechtrager/cctools-port/archive
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=$(package)_$($(package)_version).tar.gz
$(package)_sha256_hash=6af00c1afcd9db3ef484a460012154d17f890a27240c5ac38573be2c145e9df4
$(package)_build_subdir=cctools
$(package)_dependencies=native_clang
$(package)_patches=ignore-otool.diff

$(package)_libtapi_version=640b4623929c923c0468143ff2a363a48665fa54
$(package)_libtapi_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_libtapi_download_file=$($(package)_libtapi_version).tar.gz
$(package)_libtapi_file_name=$(package)_libtapi_$($(package)_libtapi_version).tar.gz
$(package)_libtapi_sha256_hash=d9a4ce85314853bb14938b36d4584b15b36420869d4fc0ab71eee2178e911b04

$(package)_extra_sources += $($(package)_libtapi_file_name)

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_libtapi_download_path),$($(package)_libtapi_download_file),$($(package)_libtapi_file_name),$($(package)_libtapi_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_libtapi_sha256_hash)  $($(package)_source_dir)/$($(package)_libtapi_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir -p libtapi && \
  tar --no-same-owner --strip-components=1 -C libtapi -xf $($(package)_source_dir)/$($(package)_libtapi_file_name) && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source)
endef

define $(package)_set_vars
  $(package)_config_opts=--target=$(host) --with-libtapi=$($(package)_extract_dir)
  $(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
  $(package)_config_opts+=--enable-lto-support --with-llvm-config=$(build_prefix)/bin/llvm-config
  $(package)_cc=$(build_prefix)/bin/clang
  $(package)_cxx=$(build_prefix)/bin/clang++
  $(package)_cxxflags+=-Wno-vla-extension
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/ignore-otool.diff && \
  cd $($(package)_build_subdir); DO_NOT_UPDATE_CONFIG_SCRIPTS=1 ./autogen.sh
endef

define $(package)_config_cmds
  rm -f $(build_prefix)/lib/libc++abi.so* && \
  CC=$($(package)_cc) CXX=$($(package)_cxx) INSTALLPREFIX=$($(package)_extract_dir) ../libtapi/build.sh && \
  CC=$($(package)_cc) CXX=$($(package)_cxx) INSTALLPREFIX=$($(package)_extract_dir) ../libtapi/install.sh && \
  $($(package)_config_env) $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install && \
  mkdir -p $($(package)_staging_prefix_dir)/lib/ && \
  if [ -f $($(package)_extract_dir)/lib/libtapi.so.6 ]; then \
    cp $($(package)_extract_dir)/lib/libtapi.so.6 $($(package)_staging_prefix_dir)/lib/; \
  elif [ -f $($(package)_extract_dir)/lib/libtapi.so ]; then \
    cp $($(package)_extract_dir)/lib/libtapi.so* $($(package)_staging_prefix_dir)/lib/; \
  fi && \
  printf '#!/bin/bash\nexec $(build_prefix)/bin/clang -target $(host) -mmacosx-version-min=10.14 --sysroot $(OSX_SDK) -mlinker-version=530 -B$(build_prefix)/bin "$$$$@"\n' > $($(package)_staging_prefix_dir)/bin/$(host)-cc && \
  chmod +x $($(package)_staging_prefix_dir)/bin/$(host)-cc
endef
