distclean: clean

clean:
	@echo "Cleaning..."
	${RM} -f logs/*.log logs/*.debug run/*.pid run/*.pipe
	${RM} -f ${extra_clean}
	${MAKE} ${extra_clean_targets}
	@echo "Done cleaning!"
