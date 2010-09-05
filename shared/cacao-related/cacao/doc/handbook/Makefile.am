## doc/handbook/Makefile.am
##
## Copyright (C) 1996-2005, 2006 R. Grafl, A. Krall, C. Kruegel,
## C. Oates, R. Obermaisser, M. Platter, M. Probst, S. Ring,
## E. Steiner, C. Thalinger, D. Thuernbeck, P. Tomsich, C. Ullrich,
## J. Wenninger, Institut f. Computersprachen - TU Wien
##
## This file is part of CACAO.
##
## This program is free software; you can redistribute it and/or
## modify it under the terms of the GNU General Public License as
## published by the Free Software Foundation; either version 2, or (at
## your option) any later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
## 02110-1301, USA.
##
## Contact: cacao@cacaojvm.org
##
## Authors: Christian Thalinger
##
## Changes:
##
## $Id: Makefile.am 5016 2006-06-06 14:32:01Z michi $

## Process this file with automake to produce Makefile.in

dist_noinst_DATA = \
	alpha.tex \
	arm.tex \
	cacao.tex \
	inlining.tex \
	intro.tex \
	java.bib \
	jit.tex \
	library.tex \
	loader.tex \
	loopopt.tex \
	mips.tex \
	native.tex \
	overview.tex \
	powerpc.tex \
	reflection.tex \
	runtime.tex \
	threads.tex \
	verification.tex \
	x86.tex \
	x86_64.tex

handbook:
	latex cacao
	bibtex cacao
	latex cacao
	latex cacao

clean:
	rm -f *.aux *.bbl *.blg *.dvi *.lof *.log *.lot *.toc


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
