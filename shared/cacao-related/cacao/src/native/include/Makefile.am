## src/native/include/Makefile.am
##
## Copyright (C) 1996-2005, 2006, 2007 R. Grafl, A. Krall, C. Kruegel,
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
## $Id: Makefile.am 7596M 2007-12-10 18:23:05Z (local) $

## Process this file with automake to produce Makefile.in

COMMON_HEADER_FILES = \
	java_lang_Class.h \
	java_lang_Object.h \
	java_lang_String.h \
	java_lang_Thread.h \
	java_lang_Throwable.h \
	\
	java_io_InputStream.h \
	java_io_PrintStream.h \
	\
	java_lang_Boolean.h \
	java_lang_Byte.h \
	java_lang_Character.h \
	java_lang_Double.h \
	java_lang_Float.h \
	java_lang_Integer.h \
	java_lang_Long.h \
	java_lang_Short.h \
	java_util_Vector.h

JAVASE_HEADER_FILES = \
	java_lang_ClassLoader.h \
	java_lang_Cloneable.h \
	java_lang_ThreadGroup.h \
	java_lang_VMThread.h \
	java_lang_VMThrowable.h \
	java_util_Properties.h \
	\
	gnu_classpath_Pointer.h \
	gnu_classpath_Pointer32.h \
	gnu_classpath_Pointer64.h \
	java_io_File.h \
	java_lang_Process.h \
	java_lang_StackTraceElement.h \
	java_lang_VMObject.h \
	java_lang_reflect_Constructor.h \
	java_lang_reflect_Field.h \
	java_lang_reflect_Method.h \
	java_nio_Buffer.h \
	java_nio_DirectByteBufferImpl.h \
	java_security_ProtectionDomain.h \
	sun_misc_Unsafe.h

JAVAME_CLDC1_1_HEADER_FILES = \
	com_sun_cldc_io_ResourceInputStream.h \
	com_sun_cldc_io_j2me_socket_Protocol.h \
	com_sun_cldchi_io_ConsoleOutputStream.h \
	com_sun_cldchi_jvm_JVM.h \
	java_lang_Math.h \
	java_lang_Runtime.h \
	java_lang_System.h

JVMTI_HEADER_FILES = \
	gnu_classpath_jdwp_VMFrame.h \
	gnu_classpath_jdwp_VMMethod.h \
	gnu_classpath_jdwp_VMVirtualMachine.h \
	gnu_classpath_jdwp_event_EventRequest.h \
	gnu_classpath_jdwp_util_VariableTable.h \
	java_nio_ByteBuffer.h

ADDITIONAL_IMPLEMENTED_VM_CLASSES_HEADER_FILES = \
	gnu_classpath_VMStackWalker.h \
	gnu_classpath_VMSystemProperties.h \
	gnu_java_lang_management_VMClassLoadingMXBeanImpl.h \
	gnu_java_lang_management_VMMemoryMXBeanImpl.h \
	gnu_java_lang_management_VMRuntimeMXBeanImpl.h \
	gnu_java_lang_management_VMThreadMXBeanImpl.h \
	java_lang_VMClass.h \
	java_lang_VMClassLoader.h \
	java_lang_VMRuntime.h \
	java_lang_VMString.h \
	java_lang_VMSystem.h \
	java_lang_management_VMManagementFactory.h \
	java_lang_management_MemoryUsage.h \
	java_lang_management_ThreadInfo.h \
	java_lang_reflect_VMProxy.h \
	java_security_VMAccessController.h

ADDITIONAL_STATIC_CLASSPATH_HEADER_FILES = \
	java_net_DatagramPacket.h \
	java_net_InetAddress.h \
	\
	gnu_java_net_PlainDatagramSocketImpl.h \
	gnu_java_net_PlainSocketImpl.h \
	gnu_java_nio_PipeImpl.h \
	gnu_java_nio_channels_FileChannelImpl.h \
	gnu_java_nio_charset_iconv_IconvEncoder.h \
	gnu_java_nio_charset_iconv_IconvDecoder.h \
	java_lang_VMProcess.h \
	java_net_SocketImpl.h \
	java_nio_MappedByteBufferImpl.h \
	java_nio_channels_spi_SelectorProvider.h

CLEANFILES = \
	$(COMMON_HEADER_FILES) \
	$(JAVASE_HEADER_FILES) \
	$(JAVAME_CLDC1_1_HEADER_FILES) \
	$(JVMTI_HEADER_FILES) \
	$(ADDITIONAL_IMPLEMENTED_VM_CLASSES_HEADER_FILES) \
	$(ADDITIONAL_STATIC_CLASSPATH_HEADER_FILES)

DO_HEADER_FILES = \
	$(COMMON_HEADER_FILES)

if ENABLE_JAVASE
DO_HEADER_FILES += \
	$(JAVASE_HEADER_FILES) \
	$(ADDITIONAL_IMPLEMENTED_VM_CLASSES_HEADER_FILES)
endif

if ENABLE_JAVAME_CLDC1_1
DO_HEADER_FILES += \
	$(JAVAME_CLDC1_1_HEADER_FILES)
endif

if ENABLE_JVMTI
DO_HEADER_FILES += \
	$(GEN_JVMTI_HEADER_FILES)
endif

if WITH_STATIC_CLASSPATH
DO_HEADER_FILES += \
	$(ADDITIONAL_STATIC_CLASSPATH_HEADER_FILES)
endif

if WITH_CLASSPATH_GNU
CLASSPATH = $(top_builddir)/src/lib/classes/:$(CLASSPATH_CLASSES)
VM_ZIP = $(top_builddir)/src/lib/vm.zip
else
CLASSPATH = $(CLASSPATH_CLASSES):$(CLASSPATH_PREFIX)/share/classpath/glibj.zip
endif

noinst_DATA = $(DO_HEADER_FILES)

$(DO_HEADER_FILES): $(CACAOH) $(VM_ZIP) $(CLASSPATH_CLASSES)
	@class=`echo $@ | sed -e 's/\.h$$//' -e 's/_/\./g'`; \
	echo "$(CACAOH) -bootclasspath $(CLASSPATH) -d . $$class"; \
	$(CACAOH) -bootclasspath $(CLASSPATH) -d . $$class


## Local variables:
## mode: Makefile
## indent-tabs-mode: t
## c-basic-offset: 4
## tab-width: 8
## compile-command: "automake --add-missing"
## End:
