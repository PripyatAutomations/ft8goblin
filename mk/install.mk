install: ${real_bins} ${real_libs} ${extra_install_targets}
	@for i in ${real_bins}; do \
		install -m 0755 $$i ${bin_install_path}; \
	done
	@for i in ${real_libs}; do \
		install -m 0644 $$i ${lib_install_path}; \
	done
	@echo "Rebuilding dynamic linker cache..."
	ldconfig
