# Minimal makefile for Sphinx documentation (bilingual: zh + en trees).
#   make html     -> doxygen (C++ API XML) + build both language trees
#   make zh / en  -> build a single language tree (doxygen runs first)
#   make livehtml -> live-reload build of both trees
#   make clean    -> remove build outputs
#
# The C++ API chapter needs the Doxygen XML first: it is generated from
# docs/doxygen/Doxyfile by the `doxygen` target (docs.yml CI does the same).

SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build
DOXYGENDIR    = ../doxygen

.PHONY: help doxygen html zh en livehtml clean

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)/zh" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Generate the Doxygen XML for the C++ API (rendered by Breathe; must run before sphinx-build)
doxygen:
	cd "$(DOXYGENDIR)" && doxygen Doxyfile

# -b html, NOT -M html: make mode (-M) appends html/ + doctrees/ under the
# given outdir, nesting the site one level too deep
# (build/html/zh/html/... -> /SparQSim/en/index.html 404s)
html: doxygen
	$(SPHINXBUILD) -b html -d "$(BUILDDIR)/doctrees/zh" "$(SOURCEDIR)/zh" "$(BUILDDIR)/html/zh" $(SPHINXOPTS) $(O)
	$(SPHINXBUILD) -b html -d "$(BUILDDIR)/doctrees/en" "$(SOURCEDIR)/en" "$(BUILDDIR)/html/en" $(SPHINXOPTS) $(O)

zh: doxygen
	$(SPHINXBUILD) -b html -d "$(BUILDDIR)/doctrees/zh" "$(SOURCEDIR)/zh" "$(BUILDDIR)/html/zh" $(SPHINXOPTS) $(O)

en: doxygen
	$(SPHINXBUILD) -b html -d "$(BUILDDIR)/doctrees/en" "$(SOURCEDIR)/en" "$(BUILDDIR)/html/en" $(SPHINXOPTS) $(O)

livehtml:
	sphinx-autobuild "$(SOURCEDIR)/zh" "$(BUILDDIR)/html/zh" $(SPHINXOPTS)
	sphinx-autobuild "$(SOURCEDIR)/en" "$(BUILDDIR)/html/en" $(SPHINXOPTS)

clean:
	rm -rf $(BUILDDIR)
