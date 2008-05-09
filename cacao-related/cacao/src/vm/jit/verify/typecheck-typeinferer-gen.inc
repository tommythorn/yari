#define GENERATED

  GENERATED  case ICMD_NOP:
  GENERATED  case ICMD_INLINE_START:
  GENERATED  case ICMD_INLINE_END:
  GENERATED  case ICMD_INLINE_BODY:
  GENERATED  	/* (--) */
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ACONST:
  GENERATED  	/* (--A) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 355 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (IPTR->flags.bits & INS_FLAG_CLASS) {
  GENERATED  		/* a java.lang.Class reference */
  GENERATED  		TYPEINFO_INIT_JAVA_LANG_CLASS(DST->typeinfo,IPTR->sx.val.c);
  GENERATED  	}
  GENERATED  	else {
  GENERATED  		if (IPTR->sx.val.anyptr == NULL)
  GENERATED  			TYPEINFO_INIT_NULLTYPE(DST->typeinfo);
  GENERATED  		else {
  GENERATED  			/* string constant (or constant for builtin function) */
  GENERATED  			typeinfo_init_classinfo(&(DST->typeinfo),class_java_lang_String);
  GENERATED  		}
  GENERATED  	}
  GENERATED  
#            	line 33 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	VAROP(iptr->dst)->type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_CHECKNULL:
  GENERATED  	/* (A--A) */
  GENERATED  	maythrow = true;
  GENERATED  	VAROP(iptr->dst)->type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ICONST:
  GENERATED  case ICMD_IDIVPOW2:
  GENERATED  case ICMD_LCMPCONST:
  GENERATED  case ICMD_IADDCONST:
  GENERATED  case ICMD_ISUBCONST:
  GENERATED  case ICMD_IMULCONST:
  GENERATED  case ICMD_IANDCONST:
  GENERATED  case ICMD_IORCONST:
  GENERATED  case ICMD_IXORCONST:
  GENERATED  case ICMD_ISHLCONST:
  GENERATED  case ICMD_ISHRCONST:
  GENERATED  case ICMD_IUSHRCONST:
  GENERATED  case ICMD_IREMPOW2:
  GENERATED  case ICMD_IADD:
  GENERATED  case ICMD_ISUB:
  GENERATED  case ICMD_IMUL:
  GENERATED  case ICMD_INEG:
  GENERATED  case ICMD_ISHL:
  GENERATED  case ICMD_ISHR:
  GENERATED  case ICMD_IUSHR:
  GENERATED  case ICMD_IAND:
  GENERATED  case ICMD_IOR:
  GENERATED  case ICMD_IXOR:
  GENERATED  case ICMD_L2I:
  GENERATED  case ICMD_F2I:
  GENERATED  case ICMD_D2I:
  GENERATED  case ICMD_INT2BYTE:
  GENERATED  case ICMD_INT2CHAR:
  GENERATED  case ICMD_INT2SHORT:
  GENERATED  case ICMD_LCMP:
  GENERATED  case ICMD_FCMPL:
  GENERATED  case ICMD_FCMPG:
  GENERATED  case ICMD_DCMPL:
  GENERATED  case ICMD_DCMPG:
  GENERATED  case ICMD_IMULPOW2:
  GENERATED  	/* (--I), (I--I), (L--I), (II--I), (F--I), (D--I), (LL--I), (FF--I), (DD--I) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LDIVPOW2:
  GENERATED  case ICMD_LCONST:
  GENERATED  case ICMD_LADDCONST:
  GENERATED  case ICMD_LSUBCONST:
  GENERATED  case ICMD_LMULCONST:
  GENERATED  case ICMD_LANDCONST:
  GENERATED  case ICMD_LORCONST:
  GENERATED  case ICMD_LXORCONST:
  GENERATED  case ICMD_LSHLCONST:
  GENERATED  case ICMD_LSHRCONST:
  GENERATED  case ICMD_LUSHRCONST:
  GENERATED  case ICMD_LREMPOW2:
  GENERATED  case ICMD_LADD:
  GENERATED  case ICMD_LSUB:
  GENERATED  case ICMD_LMUL:
  GENERATED  case ICMD_LNEG:
  GENERATED  case ICMD_LSHL:
  GENERATED  case ICMD_LSHR:
  GENERATED  case ICMD_LUSHR:
  GENERATED  case ICMD_LAND:
  GENERATED  case ICMD_LOR:
  GENERATED  case ICMD_LXOR:
  GENERATED  case ICMD_I2L:
  GENERATED  case ICMD_F2L:
  GENERATED  case ICMD_D2L:
  GENERATED  case ICMD_LMULPOW2:
  GENERATED  	/* (L--L), (--L), (LL--L), (LI--L), (I--L), (F--L), (D--L) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FCONST:
  GENERATED  case ICMD_FADD:
  GENERATED  case ICMD_FSUB:
  GENERATED  case ICMD_FMUL:
  GENERATED  case ICMD_FDIV:
  GENERATED  case ICMD_FREM:
  GENERATED  case ICMD_FNEG:
  GENERATED  case ICMD_I2F:
  GENERATED  case ICMD_L2F:
  GENERATED  case ICMD_D2F:
  GENERATED  	/* (--F), (FF--F), (F--F), (I--F), (L--F), (D--F) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DCONST:
  GENERATED  case ICMD_DADD:
  GENERATED  case ICMD_DSUB:
  GENERATED  case ICMD_DMUL:
  GENERATED  case ICMD_DDIV:
  GENERATED  case ICMD_DREM:
  GENERATED  case ICMD_DNEG:
  GENERATED  case ICMD_I2D:
  GENERATED  case ICMD_L2D:
  GENERATED  case ICMD_F2D:
  GENERATED  	/* (--D), (DD--D), (D--D), (I--D), (L--D), (F--D) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_COPY:
  GENERATED  	/* (1--1) */
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 90 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_stack);
  GENERATED  	COPYTYPE(IPTR->s1, IPTR->dst);
  GENERATED  	DST->type = OP1->type;
  GENERATED  
#            	line 163 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_MOVE:
  GENERATED  	/* (1--1) */
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 89 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_stack);
  GENERATED  	COPYTYPE(IPTR->s1, IPTR->dst);
  GENERATED  	DST->type = OP1->type;
  GENERATED  
#            	line 186 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ILOAD:
  GENERATED  	/* (--I) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LLOAD:
  GENERATED  	/* (--L) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FLOAD:
  GENERATED  	/* (--F) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DLOAD:
  GENERATED  	/* (--D) */
  GENERATED  	VAROP(iptr->dst)->type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ALOAD:
  GENERATED  	/* (--A) */
  GENERATED  
#            	define OP1  VAROP(IPTR->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 99 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_aload);
  GENERATED  
  GENERATED  
#            if !defined(TYPECHECK_TYPEINFERER)
  GENERATED  	/* loading a returnAddress is not allowed */
  GENERATED  	if (!TYPEDESC_IS_REFERENCE(*OP1)) {
  GENERATED  		VERIFY_ERROR("illegal instruction: ALOAD loading non-reference");
  GENERATED  	}
  GENERATED  
#            endif
  GENERATED  	TYPEINFO_COPY(OP1->typeinfo,DST->typeinfo);
  GENERATED  
#            	line 241 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	VAROP(iptr->dst)->type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IALOAD:
  GENERATED  case ICMD_BALOAD:
  GENERATED  case ICMD_CALOAD:
  GENERATED  case ICMD_SALOAD:
  GENERATED  case ICMD_IDIV:
  GENERATED  case ICMD_IREM:
  GENERATED  case ICMD_ARRAYLENGTH:
  GENERATED  case ICMD_INSTANCEOF:
  GENERATED  	/* (AI--I), (II--I), (A--I) */
  GENERATED  	maythrow = true;
  GENERATED  	VAROP(iptr->dst)->type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LALOAD:
  GENERATED  case ICMD_LDIV:
  GENERATED  case ICMD_LREM:
  GENERATED  	/* (AI--L), (LL--L) */
  GENERATED  	maythrow = true;
  GENERATED  	VAROP(iptr->dst)->type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FALOAD:
  GENERATED  	/* (AI--F) */
  GENERATED  	maythrow = true;
  GENERATED  	VAROP(iptr->dst)->type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DALOAD:
  GENERATED  	/* (AI--D) */
  GENERATED  	maythrow = true;
  GENERATED  	VAROP(iptr->dst)->type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_AALOAD:
  GENERATED  	/* (AI--A) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 121 "src/vm/jit/verify/icmds.c"
  GENERATED  
#            if !defined(TYPECHECK_TYPEINFERER)
  GENERATED  	if (!TYPEINFO_MAYBE_ARRAY_OF_REFS(OP1->typeinfo))
  GENERATED  		VERIFY_ERROR("illegal instruction: AALOAD on non-reference array");
  GENERATED  
#            endif
  GENERATED  
  GENERATED  	if (!typeinfo_init_component(&OP1->typeinfo,&DST->typeinfo))
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 309 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	VAROP(iptr->dst)->type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ISTORE:
  GENERATED  	/* (I--) */
  GENERATED  	STORE_LOCAL(TYPE_INT, IPTR->dst.varindex);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LSTORE:
  GENERATED  	/* (L--) */
  GENERATED  	STORE_LOCAL_2_WORD(TYPE_LNG, IPTR->dst.varindex);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FSTORE:
  GENERATED  	/* (F--) */
  GENERATED  	STORE_LOCAL(TYPE_FLT, IPTR->dst.varindex);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DSTORE:
  GENERATED  	/* (D--) */
  GENERATED  	STORE_LOCAL_2_WORD(TYPE_DBL, IPTR->dst.varindex);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ASTORE:
  GENERATED  	/* (A--|R--) */
  GENERATED  	STORE_LOCAL(VAROP(iptr->s1)->type, IPTR->dst.varindex);
  GENERATED  
#            	define DST  VAROP(IPTR->dst)
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
  GENERATED  
#            	line 114 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPEINFO_COPY(OP1->typeinfo, DST->typeinfo);
  GENERATED  
#            	line 356 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IF_LEQ:
  GENERATED  case ICMD_IF_LNE:
  GENERATED  case ICMD_IF_LLT:
  GENERATED  case ICMD_IF_LGE:
  GENERATED  case ICMD_IF_LGT:
  GENERATED  case ICMD_IF_LLE:
  GENERATED  case ICMD_IF_LCMPEQ:
  GENERATED  case ICMD_IF_LCMPNE:
  GENERATED  case ICMD_IF_LCMPLT:
  GENERATED  case ICMD_IF_LCMPGE:
  GENERATED  case ICMD_IF_LCMPGT:
  GENERATED  case ICMD_IF_LCMPLE:
  GENERATED  case ICMD_IFEQ:
  GENERATED  case ICMD_IFNE:
  GENERATED  case ICMD_IFLT:
  GENERATED  case ICMD_IFGE:
  GENERATED  case ICMD_IFGT:
  GENERATED  case ICMD_IFLE:
  GENERATED  case ICMD_IF_ICMPEQ:
  GENERATED  case ICMD_IF_ICMPNE:
  GENERATED  case ICMD_IF_ICMPLT:
  GENERATED  case ICMD_IF_ICMPGE:
  GENERATED  case ICMD_IF_ICMPGT:
  GENERATED  case ICMD_IF_ICMPLE:
  GENERATED  case ICMD_IF_ACMPEQ:
  GENERATED  case ICMD_IF_ACMPNE:
  GENERATED  case ICMD_IFNULL:
  GENERATED  case ICMD_IFNONNULL:
  GENERATED  	/* (L--), (LL--), (I--), (II--), (AA--), (A--) */
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
  GENERATED  
#            	line 414 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 406 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IASTORE:
  GENERATED  case ICMD_LASTORE:
  GENERATED  case ICMD_FASTORE:
  GENERATED  case ICMD_DASTORE:
  GENERATED  case ICMD_AASTORE:
  GENERATED  case ICMD_BASTORE:
  GENERATED  case ICMD_CASTORE:
  GENERATED  case ICMD_SASTORE:
  GENERATED  case ICMD_PUTSTATIC:
  GENERATED  case ICMD_PUTFIELD:
  GENERATED  case ICMD_IASTORECONST:
  GENERATED  case ICMD_LASTORECONST:
  GENERATED  case ICMD_FASTORECONST:
  GENERATED  case ICMD_DASTORECONST:
  GENERATED  case ICMD_AASTORECONST:
  GENERATED  case ICMD_BASTORECONST:
  GENERATED  case ICMD_CASTORECONST:
  GENERATED  case ICMD_SASTORECONST:
  GENERATED  case ICMD_PUTSTATICCONST:
  GENERATED  case ICMD_PUTFIELDCONST:
  GENERATED  	/* (AII--), (AIL--), (AIF--), (AID--), (AIA--), (1--|2--), (A1--|A2--), (AI--), (--), (A--) */
  GENERATED  	maythrow = true;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_POP:
  GENERATED  	/* (1--) */
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_POP2:
  GENERATED  	/* (11--|2--) */
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IINC:
  GENERATED  	/* (--) */
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_GOTO:
  GENERATED  	/* (--) */
  GENERATED  	superblockend = true;
  GENERATED  
  GENERATED  
#            	line 396 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 466 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_JSR:
  GENERATED  	/* (--R) */
  GENERATED  	superblockend = true;
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 587 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPEINFO_INIT_RETURNADDRESS(DST->typeinfo, BPTR->next);
  GENERATED  	REACH(IPTR->sx.s23.s3.jsrtarget);
  GENERATED  
#            	line 482 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	VAROP(iptr->dst)->type = TYPE_RET;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_RET:
  GENERATED  	/* (--) */
  GENERATED  	superblockend = true;
  GENERATED  
  GENERATED  
#            	line 604 "src/vm/jit/verify/icmds.c"
  GENERATED  
#            if !defined(TYPECHECK_TYPEINFERER)
  GENERATED  	/* check returnAddress variable */
  GENERATED  	if (!typevector_checkretaddr(jd->var,IPTR->s1.varindex))
  GENERATED  		VERIFY_ERROR("illegal instruction: RET using non-returnAddress variable");
  GENERATED  
#            endif
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 506 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_TABLESWITCH:
  GENERATED  	/* (I--) */
  GENERATED  	superblockend = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
  GENERATED  
#            	line 463 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_switch);
  GENERATED  
  GENERATED  	table = IPTR->dst.table;
  GENERATED  	i = IPTR->sx.s23.s3.tablehigh
  GENERATED  	- IPTR->sx.s23.s2.tablelow + 1 + 1; /* plus default */
  GENERATED  
  GENERATED  	while (--i >= 0) {
  GENERATED  		REACH(*table);
  GENERATED  		table++;
  GENERATED  	}
  GENERATED  
  GENERATED  	LOG("switch done");
  GENERATED  
#            	line 533 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LOOKUPSWITCH:
  GENERATED  	/* (I--) */
  GENERATED  	superblockend = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
  GENERATED  
#            	line 479 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_switch);
  GENERATED  
  GENERATED  	lookup = IPTR->dst.lookup;
  GENERATED  	i = IPTR->sx.s23.s2.lookupcount;
  GENERATED  	REACH(IPTR->sx.s23.s3.lookupdefault);
  GENERATED  
  GENERATED  	while (--i >= 0) {
  GENERATED  		REACH(lookup->target);
  GENERATED  		lookup++;
  GENERATED  	}
  GENERATED  
  GENERATED  	LOG("switch done");
  GENERATED  
#            	line 563 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IRETURN:
  GENERATED  case ICMD_LRETURN:
  GENERATED  case ICMD_FRETURN:
  GENERATED  case ICMD_DRETURN:
  GENERATED  case ICMD_RETURN:
  GENERATED  	/* (I--), (L--), (F--), (D--), (--) */
  GENERATED  	superblockend = true;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ARETURN:
  GENERATED  case ICMD_ATHROW:
  GENERATED  	/* (A--) */
  GENERATED  	maythrow = true;
  GENERATED  	superblockend = true;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_GETSTATIC:
  GENERATED  	/* (--1|--2) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 230 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!handle_fieldaccess(state, NULL, NULL))
  GENERATED  		return false;
  GENERATED  	maythrow = true;
  GENERATED  
#            	line 601 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_GETFIELD:
  GENERATED  	/* (A--1|A--2) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 224 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!handle_fieldaccess(state, VAROP(iptr->s1), NULL))
  GENERATED  		return false;
  GENERATED  	maythrow = true;
  GENERATED  
#            	line 623 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_INVOKEVIRTUAL:
  GENERATED  case ICMD_INVOKESPECIAL:
  GENERATED  case ICMD_INVOKESTATIC:
  GENERATED  case ICMD_INVOKEINTERFACE:
  GENERATED  	/* (.--1|.--2|.--) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
  GENERATED  
#            	line 626 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_invoke);
  GENERATED  	if (!handle_invocation(state))
  GENERATED  		EXCEPTION;
  GENERATED  	TYPECHECK_COUNTIF(INSTRUCTION_IS_UNRESOLVED(IPTR), stat_ins_invoke_unresolved);
  GENERATED  
#            	line 649 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_CHECKCAST:
  GENERATED  	/* (A--A) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 373 "src/vm/jit/verify/icmds.c"
  GENERATED  
#            if !defined(TYPECHECK_TYPEINFERER)
  GENERATED  	/* returnAddress is not allowed */
  GENERATED  	if (!TYPEINFO_IS_REFERENCE(OP1->typeinfo))
  GENERATED  		VERIFY_ERROR("Illegal instruction: CHECKCAST on non-reference");
  GENERATED  
#            endif
  GENERATED  
  GENERATED      /* XXX only if narrower */
  GENERATED  	if (!typeinfo_init_class(&(DST->typeinfo),IPTR->sx.s23.s3.c))
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 679 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	VAROP(iptr->dst)->type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_MULTIANEWARRAY:
  GENERATED  	/* (.--A) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
#            	define DST  VAROP(iptr->dst)
  GENERATED  
  GENERATED  
#            	line 669 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!handle_multianewarray(STATE))
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 703 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	VAROP(iptr->dst)->type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_BUILTIN:
  GENERATED  	/* (.--1|.--2|.--) */
  GENERATED  	maythrow = true;
  GENERATED  
#            	define OP1  VAROP(iptr->s1)
  GENERATED  
  GENERATED  
#            	line 684 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_builtin);
  GENERATED  	if (!handle_builtin(state))
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 726 "src/vm/jit/verify/typecheck-typeinferer-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
#undef GENERATED
/* vim:filetype=c:
 */
