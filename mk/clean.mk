distclean: clean ${extra_distclean_targets}
	${RM} -f ${extra_distclean}

clean: ${extra_clean_targets}
	@echo "Cleaning..."
	${RM} -f logs/*.log logs/*.debug run/*.pid run/*.pipe
	${RM} -f ${extra_clean}
	@echo "Done cleaning!"
