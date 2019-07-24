GLA11Y_OUTPUT = ui-a11y.err
GLA11Y_SUPPR  = ui-a11y.suppr
GLA11Y_FALSE  = ui-a11y.false

a11y_verbose = $(a11y_verbose_@AM_V@)
a11y_verbose_ = $(a11y_verbose_@AM_DEFAULT_V@)
a11y_verbose_0 = @echo "  A11Y    " $@;
a11y_verbose_1 = 


all-local: $(GLA11Y_OUTPUT)
$(GLA11Y_OUTPUT): $(ui_files)
	$(a11y_verbose) $(GLA11Y) -P $(srcdir)/ -f $(srcdir)/$(GLA11Y_FALSE) -s $(srcdir)/$(GLA11Y_SUPPR) -o $@ $(ui_files:%=$(srcdir)/%)

clean-local: clean-local-check
clean-local-check:
	-rm -f $(GLA11Y_OUTPUT)

.PHONY: clean-local-check
