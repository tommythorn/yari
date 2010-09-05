## tests/regression/jasmin/Makefile.am
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
##          Edwin Steiner
##
## $Id$

## Process this file with automake to produce Makefile.in

JAVA      = $(top_builddir)/src/cacao/cacao

if WITH_CLASSPATH_GNU
JAVAFLAGS = -Xbootclasspath:$(top_builddir)/src/lib/classes/:$(CLASSPATH_CLASSES)
else
JAVAFLAGS = -Xbootclasspath:$(CLASSPATH_CLASSES)
endif

JASMIN_TESTS = \
	$(srcdir)/test_coalesce_simple_store.j \
	$(srcdir)/test_dup2.j \
	$(srcdir)/test_dup2_x1.j \
	$(srcdir)/test_dup2_x2.j \
	$(srcdir)/test_dup.j \
	$(srcdir)/test_dup_x1.j \
	$(srcdir)/test_dup_x1_interface_slots.j \
	$(srcdir)/test_dup_x2.j \
	$(srcdir)/test_dup_x2_interface_slots.j \
	$(srcdir)/test_dup_x2_to_dup_x1.j \
	$(srcdir)/test.j \
	$(srcdir)/test_iinc.j \
	$(srcdir)/test_load_store_conflict_by_exception.j \
	$(srcdir)/test_load_store_conflict_by_exception_not_thrown.j \
	$(srcdir)/test_load_store_conflict.j \
	$(srcdir)/test_load_store_conflict_via_dup.j \
	$(srcdir)/test_load_store_conflict_via_swap.j \
	$(srcdir)/test_many_dup.j \
	$(srcdir)/test_many_dup_x1.j \
	$(srcdir)/test_many_dup_x2.j \
	$(srcdir)/test_many_dup2.j \
	$(srcdir)/test_many_dup2_x1.j \
	$(srcdir)/test_many_dup2_x2.j \
	$(srcdir)/test_many_monitors.j \
	$(srcdir)/test_many_swap.j \
	$(srcdir)/test_no_store_load_conflict.j \
	$(srcdir)/test_no_store_store_conflict.j \
	$(srcdir)/test_nullpointerexception_monitorexit.j \
	$(srcdir)/test_simple_load_store.j \
	$(srcdir)/test_store_load_conflict.j \
	$(srcdir)/test_store_store_conflict_2.j \
	$(srcdir)/test_store_store_conflict.j \
	$(srcdir)/test_swap_interface_slots.j \
	$(srcdir)/test_swap.j \
	$(srcdir)/test_swap_locals.j \
	$(srcdir)/test_verify_fail_aload_retaddress.j \
	$(srcdir)/test_verify_fail_areturn_wrong_reftype.j \
	$(srcdir)/test_verify_fail_athrow_wrong_reftype.j \
	$(srcdir)/test_verify_fail_athrow_wrong_reftype_unresolved.j \
	$(srcdir)/test_verify_fail_backward_with_new_on_stack.j \
	$(srcdir)/test_verify_fail_double_local_index.j \
	$(srcdir)/test_verify_fail_double_overwritten.j \
	$(srcdir)/test_verify_fail_getfield_basic_type_instance.j \
	$(srcdir)/test_verify_fail_getfield_basic_type_lookup.j \
	$(srcdir)/test_verify_fail_getfield_basic_type_value.j \
	$(srcdir)/test_verify_fail_handler_bad_local.j \
	$(srcdir)/test_verify_fail_init_nullpointer.j \
	$(srcdir)/test_verify_fail_invoke_basic_type.j \
	$(srcdir)/test_verify_fail_invoke_return_basic_type.j \
	$(srcdir)/test_verify_fail_ireturn_wrong_type.j \
	$(srcdir)/test_verify_fail_jsr_called_with_different_stackdepths.j \
	$(srcdir)/test_verify_fail_jsr_exceptions.j \
	$(srcdir)/test_verify_fail_jsr_handler_in_sub.j \
	$(srcdir)/test_verify_fail_jsr_merge_subroutines.j \
	$(srcdir)/test_verify_fail_jsr_merge_subroutines_via_stack.j \
	$(srcdir)/test_verify_fail_jsr_polymorphic_pop.j \
	$(srcdir)/test_verify_fail_load_wrong_type.j \
	$(srcdir)/test_verify_fail_load_wrong_type_within_block.j \
	$(srcdir)/test_verify_fail_local_index.j \
	$(srcdir)/test_verify_fail_long_local_index.j \
	$(srcdir)/test_verify_fail_long_local.j \
	$(srcdir)/test_verify_fail_long_overwritten.j \
	$(srcdir)/test_verify_fail_merge_different_new_objects.j \
	$(srcdir)/test_verify_fail_merge_init_nullpointer.j \
	$(srcdir)/test_verify_fail_putfield_basic_type_instance.j \
	$(srcdir)/test_verify_fail_putfield_basic_type_lookup.j \
	$(srcdir)/test_verify_fail_putfield_basic_type_value.j \
	$(srcdir)/test_verify_fail_putfieldconst_basic_type_instance.j \
	$(srcdir)/test_verify_fail_putfieldconst_basic_type_value.j \
	$(srcdir)/test_verify_fail_putstatic_basic_type_value.j \
	$(srcdir)/test_verify_fail_putstaticconst_basic_type_value.j \
	$(srcdir)/test_verify_fail_retaddr_as_object.j \
	$(srcdir)/test_verify_fail_ret_bad_type.j \
	$(srcdir)/test_verify_fail_ret_uninit_var.j \
	$(srcdir)/test_verify_fail_split_local.j \
	$(srcdir)/test_verify_ok_jsr_handler_in_sub2.j \
	$(srcdir)/test_verify_ok_jsr_improper_nesting.j \
	$(srcdir)/test_verify_ok_jsr.j \
	$(srcdir)/test_verify_ok_jsr_multiple_blocks.j \
	$(srcdir)/test_verify_ok_jsr_pop.j \
	$(srcdir)/test_verify_ok_jsr_push.j \
	$(srcdir)/test_verify_ok_jsr_subroutine_loops_to_start.j \
	$(srcdir)/test_verify_ok_jsr_swap.j \
	$(srcdir)/test_verify_ok_jsr_through_variable.j \
	$(srcdir)/test_verify_ok_local_as_retaddr_and_reference.j \
	$(srcdir)/test_verify_ok_overwrite_local_type.j \
	$(srcdir)/test_verify_ok_untyped_local.j \
	$(srcdir)/test_verify_unspecced_ok_backward_with_new_in_local.j \
	$(srcdir)/test_verify_unspecced_ok_backward_with_new_on_stack.j \
	$(srcdir)/test_verify_unspecced_ok_new_in_local_within_try.j



EXTRA_DIST = \
	$(srcdir)/runtest \
	$(srcdir)/show \
	$(JASMIN_TESTS)

CLEANFILES = \
	*.class \
	TESTLOG TESTEXPECT TESTOUT TESTERR TESTSIA

check:
	@LD_LIBRARY_PATH=$(top_builddir)/src/cacao/.libs JAVA="$(JAVA)" JAVAFLAGS="$(JAVAFLAGS)" $(srcdir)/runtest $(JASMIN_TESTS)

## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
