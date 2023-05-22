distclean: clean ${extra_distclean_targets}
	${RM} -f ${extra_distclean}

clean: ${extra_clean_targets}
	@echo "Cleaning..."
	${RM} -f logs/*.log logs/*san.log.* logs/*.debug run/*.pid run/*.pipe
	${RM} -f ${extra_clean} ${clean}
	@echo "Done cleaning!"
