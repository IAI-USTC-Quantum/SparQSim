# Minimal makefile for Sphinx documentation

# You can set these variables from the command line.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build
DOXYGENDIR    = ../doxygen

.PHONY: help doxygen clean Makefile

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Generate the Doxygen XML for the C++ API (rendered by Breathe; must run before sphinx-build)
doxygen:
	cd "$(DOXYGENDIR)" && doxygen Doxyfile

# html depends on the Doxygen XML; the remaining targets use sphinx-build's default pattern rules
html: doxygen Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
