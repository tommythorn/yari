#define GENERATED

  GENERATED  case ICMD_NOP:
  GENERATED  	/* (--) */
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ACONST:
  GENERATED  	/* (--A) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  
#            	define DST  (&(stack[1]))
  GENERATED  	/* may use stack[1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 349 "src/vm/jit/verify/icmds.c"
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
#            	line 32 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_CHECKNULL:
  GENERATED  	/* (A--A) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ICONST:
  GENERATED  	/* (--I) */
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LCONST:
  GENERATED  	/* (--L) */
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	stack += 2;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FCONST:
  GENERATED  	/* (--F) */
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DCONST:
  GENERATED  	/* (--D) */
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	stack += 2;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ILOAD:
  GENERATED  	/* (--I) */
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_INT);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LLOAD:
  GENERATED  	/* (--L) */
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_LNG);
  GENERATED  	stack += 2;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FLOAD:
  GENERATED  	/* (--F) */
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_FLT);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DLOAD:
  GENERATED  	/* (--D) */
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_DBL);
  GENERATED  	stack += 2;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ALOAD:
  GENERATED  	/* (--A) */
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_ADR);
  GENERATED  
#            	define OP1 LOCAL_SLOT(IPTR->s1.varindex)
  GENERATED  
#            	define DST  (&(stack[1]))
  GENERATED  	/* may use stack[1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 93 "src/vm/jit/verify/icmds.c"
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
#            	line 146 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_ADR;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IALOAD:
  GENERATED  	/* (AI--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 260 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_INT))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 177 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LALOAD:
  GENERATED  	/* (AI--L) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 270 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_LONG))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 210 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FALOAD:
  GENERATED  	/* (AI--F) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 255 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_FLOAT))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 243 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DALOAD:
  GENERATED  	/* (AI--D) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 250 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_DOUBLE))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 276 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_AALOAD:
  GENERATED  	/* (AI--A) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 115 "src/vm/jit/verify/icmds.c"
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
#            	line 316 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_BALOAD:
  GENERATED  	/* (AI--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 239 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_BOOLEAN)
  GENERATED  			&& !TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_BYTE))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 349 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_CALOAD:
  GENERATED  	/* (AI--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 245 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_CHAR))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 382 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_SALOAD:
  GENERATED  	/* (AI--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  
#            	define OP2 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 265 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_SHORT))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 415 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ISTORE:
  GENERATED  	/* (I--) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	STORE_LOCAL(TYPE_INT, IPTR->dst.varindex);
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LSTORE:
  GENERATED  	/* (L--) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	STORE_LOCAL_2_WORD(TYPE_LNG, IPTR->dst.varindex);
  GENERATED  	stack += -2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FSTORE:
  GENERATED  	/* (F--) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	STORE_LOCAL(TYPE_FLT, IPTR->dst.varindex);
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DSTORE:
  GENERATED  	/* (D--) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	STORE_LOCAL_2_WORD(TYPE_DBL, IPTR->dst.varindex);
  GENERATED  	stack += -2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ASTORE:
  GENERATED  	/* (A--|R--) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  	STORE_LOCAL(OP1->type, IPTR->dst.varindex);
  GENERATED  
#            	define DST LOCAL_SLOT(IPTR->dst.varindex)
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 108 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPEINFO_COPY(OP1->typeinfo, DST->typeinfo);
  GENERATED  
#            	line 479 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IASTORE:
  GENERATED  	/* (AII--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-2]))
  GENERATED  
#            	define OP2 (&(stack[-1]))
  GENERATED  
#            	define OP3 (&(stack[0]))
  GENERATED  	/* may use stack[-2] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 296 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_INT))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 510 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -3;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LASTORE:
  GENERATED  	/* (AIL--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  
#            	define OP1 (&(stack[-3]))
  GENERATED  
#            	define OP2 (&(stack[-2]))
  GENERATED  
#            	define OP3 (&(stack[-1]))
  GENERATED  	/* may use stack[-3] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 306 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_LONG))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 543 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -4;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FASTORE:
  GENERATED  	/* (AIF--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  
#            	define OP1 (&(stack[-2]))
  GENERATED  
#            	define OP2 (&(stack[-1]))
  GENERATED  
#            	define OP3 (&(stack[0]))
  GENERATED  	/* may use stack[-2] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 291 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_FLOAT))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 576 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -3;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DASTORE:
  GENERATED  	/* (AID--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  
#            	define OP1 (&(stack[-3]))
  GENERATED  
#            	define OP2 (&(stack[-2]))
  GENERATED  
#            	define OP3 (&(stack[-1]))
  GENERATED  	/* may use stack[-3] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 286 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_DOUBLE))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 609 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -4;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_AASTORE:
  GENERATED  	/* (AIA--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  
#            	define OP1 (&(stack[-2]))
  GENERATED  
#            	define OP2 (&(stack[-1]))
  GENERATED  
#            	define OP3 (&(stack[0]))
  GENERATED  	/* may use stack[-2] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 311 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we just check the basic input types and that the           */
  GENERATED  	/* destination is an array of references. Assignability to    */
  GENERATED  	/* the actual array must be checked at runtime, each time the */
  GENERATED  	/* instruction is performed. (See builtin_canstore.)          */
  GENERATED  	if (!TYPEINFO_MAYBE_ARRAY_OF_REFS(OP1->typeinfo))
  GENERATED  		VERIFY_ERROR("illegal instruction: AASTORE to non-reference array");
  GENERATED  
#            	line 646 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -3;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_BASTORE:
  GENERATED  	/* (AII--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-2]))
  GENERATED  
#            	define OP2 (&(stack[-1]))
  GENERATED  
#            	define OP3 (&(stack[0]))
  GENERATED  	/* may use stack[-2] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 275 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_BOOLEAN)
  GENERATED  			&& !TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_BYTE))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 680 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -3;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_CASTORE:
  GENERATED  	/* (AII--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-2]))
  GENERATED  
#            	define OP2 (&(stack[-1]))
  GENERATED  
#            	define OP3 (&(stack[0]))
  GENERATED  	/* may use stack[-2] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 281 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_CHAR))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 713 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -3;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_SASTORE:
  GENERATED  	/* (AII--) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[-2]))
  GENERATED  
#            	define OP2 (&(stack[-1]))
  GENERATED  
#            	define OP3 (&(stack[0]))
  GENERATED  	/* may use stack[-2] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 301 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_PRIMITIVE_ARRAY(OP1->typeinfo,ARRAYTYPE_SHORT))
  GENERATED  		VERIFY_ERROR("Array type mismatch");
  GENERATED  
#            	line 746 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -3;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
#            	undef OP3
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_POP:
  GENERATED  	/* (1--) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 704 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we pop 1 */
  GENERATED  	CHECK_CAT1(stack[0]);
  GENERATED  
#            	line 769 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_POP2:
  GENERATED  	/* (11--|2--) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 709 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we pop either 11 or 2 */
  GENERATED  	if (IS_CAT1(stack[0]))
  GENERATED  		CHECK_CAT1(stack[-1]);
  GENERATED  
#            	line 786 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DUP:
  GENERATED  	/* (1--11) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  
#            	define DST  (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 724 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we dup 1 */
  GENERATED  	CHECK_CAT1(stack[0]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], stack[ 1]);
  GENERATED  
#            	line 807 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DUP_X1:
  GENERATED  	/* (11--111) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 731 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we dup 1 */
  GENERATED  	CHECK_CAT1(stack[0]);
  GENERATED  	/* we skip 1 */
  GENERATED  	CHECK_CAT1(stack[-1]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], stack[ 1]);
  GENERATED  	COPY_SLOT(stack[-1], stack[ 0]);
  GENERATED  	COPY_SLOT(stack[ 1], stack[-1]);
  GENERATED  
#            	line 835 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DUP_X2:
  GENERATED  	/* (111--1111|21--121) */
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	/* may use stack[-2] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 742 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we dup 1 */
  GENERATED  	CHECK_CAT1(stack[0]);
  GENERATED  	/* we skip either 11 or 2 */
  GENERATED  	if (IS_CAT1(stack[-1]))
  GENERATED  		CHECK_CAT1(stack[-2]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], stack[ 1]);
  GENERATED  	COPY_SLOT(stack[-1], stack[ 0]);
  GENERATED  	COPY_SLOT(stack[-2], stack[-1]);
  GENERATED  	COPY_SLOT(stack[ 1], stack[-2]);
  GENERATED  
#            	line 863 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DUP2:
  GENERATED  	/* (11--1111|2--22) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	/* may use stack[-1] ... stack[2] */
  GENERATED  
  GENERATED  
#            	line 755 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we dup either 11 or 2 */
  GENERATED  	if (IS_CAT1(stack[0]))
  GENERATED  		CHECK_CAT1(stack[-1]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], stack[ 2]);
  GENERATED  	COPY_SLOT(stack[-1], stack[ 1]);
  GENERATED  
#            	line 884 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DUP2_X1:
  GENERATED  	/* (111--11111|12--212) */
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	/* may use stack[-2] ... stack[2] */
  GENERATED  
  GENERATED  
#            	line 764 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we dup either 11 or 2 */
  GENERATED  	if (IS_CAT1(stack[0]))
  GENERATED  		CHECK_CAT1(stack[-1]);
  GENERATED  	/* we skip 1 */
  GENERATED  	CHECK_CAT1(stack[-2]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], stack[ 2]);
  GENERATED  	COPY_SLOT(stack[-1], stack[ 1]);
  GENERATED  	COPY_SLOT(stack[-2], stack[ 0]);
  GENERATED  	COPY_SLOT(stack[ 2], stack[-1]);
  GENERATED  	COPY_SLOT(stack[ 1], stack[-2]);
  GENERATED  
#            	line 910 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DUP2_X2:
  GENERATED  	/* (1111--111111|112--2112|22--222|211--11211) */
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_SPACE(2);
  GENERATED  	/* may use stack[-3] ... stack[2] */
  GENERATED  
  GENERATED  
#            	line 778 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* we dup either 11 or 2 */
  GENERATED  	if (IS_CAT1(stack[0]))
  GENERATED  		CHECK_CAT1(stack[-1]);
  GENERATED  	/* we skip either 11 or 2 */
  GENERATED  	if (IS_CAT1(stack[-2]))
  GENERATED  		CHECK_CAT1(stack[-3]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], stack[ 2]);
  GENERATED  	COPY_SLOT(stack[-1], stack[ 1]);
  GENERATED  	COPY_SLOT(stack[-2], stack[ 0]);
  GENERATED  	COPY_SLOT(stack[-3], stack[-1]);
  GENERATED  	COPY_SLOT(stack[ 2], stack[-2]);
  GENERATED  	COPY_SLOT(stack[ 1], stack[-3]);
  GENERATED  
#            	line 938 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += 2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_SWAP:
  GENERATED  	/* (11--11) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  
#            	define DST  (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 715 "src/vm/jit/verify/icmds.c"
  GENERATED  	CHECK_CAT1(stack[0]);
  GENERATED  	CHECK_CAT1(stack[-1]);
  GENERATED  
  GENERATED  	COPY_SLOT(stack[ 0], temp     );
  GENERATED  	COPY_SLOT(stack[-1], stack[ 0]);
  GENERATED  	COPY_SLOT(temp     , stack[-1]);
  GENERATED  
#            	line 960 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IADD:
  GENERATED  case ICMD_ISUB:
  GENERATED  case ICMD_IMUL:
  GENERATED  case ICMD_ISHL:
  GENERATED  case ICMD_ISHR:
  GENERATED  case ICMD_IUSHR:
  GENERATED  case ICMD_IAND:
  GENERATED  case ICMD_IOR:
  GENERATED  case ICMD_IXOR:
  GENERATED  	/* (II--I) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LADD:
  GENERATED  case ICMD_LSUB:
  GENERATED  case ICMD_LMUL:
  GENERATED  case ICMD_LAND:
  GENERATED  case ICMD_LOR:
  GENERATED  case ICMD_LXOR:
  GENERATED  	/* (LL--L) */
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_LNG);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	stack += -2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FADD:
  GENERATED  case ICMD_FSUB:
  GENERATED  case ICMD_FMUL:
  GENERATED  case ICMD_FDIV:
  GENERATED  case ICMD_FREM:
  GENERATED  	/* (FF--F) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_FLT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DADD:
  GENERATED  case ICMD_DSUB:
  GENERATED  case ICMD_DMUL:
  GENERATED  case ICMD_DDIV:
  GENERATED  case ICMD_DREM:
  GENERATED  	/* (DD--D) */
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_DBL);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	stack += -2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IDIV:
  GENERATED  case ICMD_IREM:
  GENERATED  	/* (II--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LDIV:
  GENERATED  case ICMD_LREM:
  GENERATED  	/* (LL--L) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_LNG);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	stack += -2;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_INEG:
  GENERATED  case ICMD_INT2BYTE:
  GENERATED  case ICMD_INT2CHAR:
  GENERATED  case ICMD_INT2SHORT:
  GENERATED  	/* (I--I) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LNEG:
  GENERATED  	/* (L--L) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FNEG:
  GENERATED  	/* (F--F) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DNEG:
  GENERATED  	/* (D--D) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LSHL:
  GENERATED  case ICMD_LSHR:
  GENERATED  case ICMD_LUSHR:
  GENERATED  	/* (LI--L) */
  GENERATED  	CHECK_STACK_DEPTH(3);
  GENERATED  	CHECK_STACK_TYPE(stack[-2], TYPE_LNG);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IINC:
  GENERATED  	/* (--) */
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_INT);
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_I2L:
  GENERATED  	/* (I--L) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_I2F:
  GENERATED  	/* (I--F) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack[0].type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_I2D:
  GENERATED  	/* (I--D) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_L2I:
  GENERATED  	/* (L--I) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_L2F:
  GENERATED  	/* (L--F) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_L2D:
  GENERATED  	/* (L--D) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_F2I:
  GENERATED  	/* (F--I) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_F2L:
  GENERATED  	/* (F--L) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_F2D:
  GENERATED  	/* (F--D) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_DBL;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_D2I:
  GENERATED  	/* (D--I) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_D2L:
  GENERATED  	/* (D--L) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	stack[0].type = TYPE_VOID;
  GENERATED  	stack[-1].type = TYPE_LNG;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_D2F:
  GENERATED  	/* (D--F) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_FLT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LCMP:
  GENERATED  	/* (LL--I) */
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_LNG);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  	stack += -3;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FCMPL:
  GENERATED  case ICMD_FCMPG:
  GENERATED  	/* (FF--I) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_FLT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  	stack += -1;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DCMPL:
  GENERATED  case ICMD_DCMPG:
  GENERATED  	/* (DD--I) */
  GENERATED  	CHECK_STACK_DEPTH(4);
  GENERATED  	CHECK_STACK_TYPE(stack[-3], TYPE_DBL);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  	stack += -3;
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IFEQ:
  GENERATED  case ICMD_IFNE:
  GENERATED  case ICMD_IFLT:
  GENERATED  case ICMD_IFGE:
  GENERATED  case ICMD_IFGT:
  GENERATED  case ICMD_IFLE:
  GENERATED  	/* (I--) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -1;
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define OP1 (&(stack[1]))
  GENERATED  	/* may use stack[1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 393 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 1264 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IF_ICMPEQ:
  GENERATED  case ICMD_IF_ICMPNE:
  GENERATED  case ICMD_IF_ICMPLT:
  GENERATED  case ICMD_IF_ICMPGE:
  GENERATED  case ICMD_IF_ICMPGT:
  GENERATED  case ICMD_IF_ICMPLE:
  GENERATED  	/* (II--) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_INT);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -2;
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define OP1 (&(stack[1]))
  GENERATED  
#            	define OP2 (&(stack[2]))
  GENERATED  	/* may use stack[1] ... stack[2] */
  GENERATED  
  GENERATED  
#            	line 399 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 1298 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IF_ACMPEQ:
  GENERATED  case ICMD_IF_ACMPNE:
  GENERATED  	/* (AA--) */
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  	stack += -2;
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define OP1 (&(stack[1]))
  GENERATED  
#            	define OP2 (&(stack[2]))
  GENERATED  	/* may use stack[1] ... stack[2] */
  GENERATED  
  GENERATED  
#            	line 405 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 1330 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef OP2
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_GOTO:
  GENERATED  	/* (--) */
  GENERATED  	superblockend = true;
  GENERATED  
  GENERATED  
#            	line 390 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 1352 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_JSR:
  GENERATED  	/* (--R) */
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_SPACE(1);
  GENERATED  	stack += 1;
  GENERATED  	stack[0].type = TYPE_RET;
  GENERATED  	/* CAUTION: stack types changed before custom code! */
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define DST  (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 586 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	tbptr = IPTR->sx.s23.s3.jsrtarget.block;
  GENERATED  
  GENERATED  	TYPEINFO_INIT_RETURNADDRESS(stack[0].typeinfo, tbptr);
  GENERATED  	REACH_BLOCK(tbptr);
  GENERATED  
  GENERATED  	stack = typecheck_stackbased_jsr(STATE, stack, stackfloor);
  GENERATED  	if (stack == NULL)
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 1381 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
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
#            	line 607 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	CHECK_LOCAL_TYPE(IPTR->s1.varindex, TYPE_RET);
  GENERATED  	if (!TYPEINFO_IS_PRIMITIVE(STATE->locals[IPTR->s1.varindex].typeinfo))
  GENERATED  		VERIFY_ERROR("illegal instruction: RET using non-returnAddress variable");
  GENERATED  
  GENERATED  	if (!typecheck_stackbased_ret(STATE, stack, stackfloor))
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 1403 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_TABLESWITCH:
  GENERATED  	/* (I--) */
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -1;
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define OP1 (&(stack[1]))
  GENERATED  	/* may use stack[1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 457 "src/vm/jit/verify/icmds.c"
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
#            	line 1435 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
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
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  	stack += -1;
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define OP1 (&(stack[1]))
  GENERATED  	/* may use stack[1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 473 "src/vm/jit/verify/icmds.c"
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
#            	line 1470 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IRETURN:
  GENERATED  	/* (I--) */
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_INT);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 539 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (STATE->returntype.type != TYPE_INT)
  GENERATED  		VERIFY_ERROR("Return type mismatch");
  GENERATED  
#            	line 1492 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	goto return_tail;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_LRETURN:
  GENERATED  	/* (L--) */
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_LNG);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 544 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (STATE->returntype.type != TYPE_LNG)
  GENERATED  		VERIFY_ERROR("Return type mismatch");
  GENERATED  
#            	line 1514 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	goto return_tail;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_FRETURN:
  GENERATED  	/* (F--) */
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_FLT);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 549 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (STATE->returntype.type != TYPE_FLT)
  GENERATED  		VERIFY_ERROR("Return type mismatch");
  GENERATED  
#            	line 1536 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	goto return_tail;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_DRETURN:
  GENERATED  	/* (D--) */
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_DBL);
  GENERATED  
#            	define OP1 (&(stack[-1]))
  GENERATED  	/* may use stack[-1] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 554 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (STATE->returntype.type != TYPE_DBL)
  GENERATED  		VERIFY_ERROR("Return type mismatch");
  GENERATED  
#            	line 1558 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	goto return_tail;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ARETURN:
  GENERATED  	/* (A--) */
  GENERATED  	maythrow = true;
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 514 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_areturn);
  GENERATED  	if (!TYPEINFO_IS_REFERENCE(OP1->typeinfo))
  GENERATED  		VERIFY_ERROR("illegal instruction: ARETURN on non-reference");
  GENERATED  
  GENERATED  	if (STATE->returntype.type != TYPE_ADR
  GENERATED  			|| (r = typeinfo_is_assignable(&OP1->typeinfo,&(STATE->returntype.typeinfo)))
  GENERATED  			== typecheck_FALSE)
  GENERATED  		VERIFY_ERROR("Return type mismatch");
  GENERATED  	if (r == typecheck_FAIL)
  GENERATED  		EXCEPTION;
  GENERATED  	if (r == typecheck_MAYBE) {
  GENERATED  		/* the check has to be postponed, we need a patcher */
  GENERATED  		TYPECHECK_COUNT(stat_ins_areturn_unresolved);
  GENERATED  		IPTR->sx.s23.s2.uc = create_unresolved_class(
  GENERATED  				METHOD,
  GENERATED  				METHOD->parseddesc->returntype.classref,
  GENERATED  				&OP1->typeinfo);
  GENERATED  		IPTR->flags.bits |= INS_FLAG_UNRESOLVED;
  GENERATED  	}
  GENERATED  
#            	line 1598 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	goto return_tail;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_RETURN:
  GENERATED  	/* (--) */
  GENERATED  	superblockend = true;
  GENERATED  
  GENERATED  
#            	line 559 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (STATE->returntype.type != TYPE_VOID)
  GENERATED  		VERIFY_ERROR("Return type mismatch");
  GENERATED  
  GENERATED  return_tail:
  GENERATED  	TYPECHECK_COUNT(stat_ins_primitive_return);
  GENERATED  
  GENERATED  	if (STATE->initmethod && METHOD->class != class_java_lang_Object) {
  GENERATED  		/* Check if the 'this' instance has been initialized. */
  GENERATED  		LOG("Checking <init> marker");
  GENERATED  
#            if defined(TYPECHECK_VARIABLESBASED)
  GENERATED  		if (!typevector_checktype(jd->var,STATE->numlocals-1,TYPE_INT))
  GENERATED  
#            else
  GENERATED  		if (STATE->locals[STATE->numlocals-1].type != TYPE_INT)
  GENERATED  
#            endif
  GENERATED  			VERIFY_ERROR("<init> method does not initialize 'this'");
  GENERATED  	}
  GENERATED  
#            	line 1632 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_GETSTATIC:
  GENERATED  	/* (--1|--2) */
  GENERATED  	maythrow = true;
  GENERATED  	/* variable number of outslots! */
  GENERATED  
  GENERATED  
#            	line 158 "src/vm/jit/verify/icmds.c"
  GENERATED  	stack = typecheck_stackbased_verify_fieldaccess(STATE, NULL, NULL, stack);
  GENERATED  	if (stack == NULL)
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 1648 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_PUTSTATIC:
  GENERATED  	/* (1--|2--) */
  GENERATED  	maythrow = true;
  GENERATED  	/* variable number of inslots! */
  GENERATED  
  GENERATED  
#            	line 140 "src/vm/jit/verify/icmds.c"
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	if (!IS_CAT1(stack[0])) {
  GENERATED  		/* (stack depth >= 2 is guaranteed) */
  GENERATED  		stack -= 1;
  GENERATED  	}
  GENERATED  	stack = typecheck_stackbased_verify_fieldaccess(STATE, NULL, stack, stack-1);
  GENERATED  	if (stack == NULL)
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 1669 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_GETFIELD:
  GENERATED  	/* (A--1|A--2) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	/* variable number of outslots! */
  GENERATED  
  GENERATED  
#            	line 151 "src/vm/jit/verify/icmds.c"
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  	stack = typecheck_stackbased_verify_fieldaccess(STATE, stack, NULL, stack-1);
  GENERATED  	if (stack == NULL)
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 1687 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_PUTFIELD:
  GENERATED  	/* (A1--|A2--) */
  GENERATED  	maythrow = true;
  GENERATED  	/* variable number of inslots! */
  GENERATED  
  GENERATED  
#            	line 128 "src/vm/jit/verify/icmds.c"
  GENERATED  	CHECK_STACK_DEPTH(2);
  GENERATED  	if (!IS_CAT1(stack[0])) {
  GENERATED  		CHECK_STACK_DEPTH(3);
  GENERATED  		stack -= 1;
  GENERATED  	}
  GENERATED  	CHECK_STACK_TYPE(stack[-1], TYPE_ADR);
  GENERATED  	stack = typecheck_stackbased_verify_fieldaccess(STATE, stack-1, stack, stack-2);
  GENERATED  	if (stack == NULL)
  GENERATED  		EXCEPTION;
  GENERATED  
#            	line 1709 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_INVOKEVIRTUAL:
  GENERATED  case ICMD_INVOKESPECIAL:
  GENERATED  case ICMD_INVOKESTATIC:
  GENERATED  case ICMD_INVOKEINTERFACE:
  GENERATED  	/* (.--1|.--2|.--) */
  GENERATED  	maythrow = true;
  GENERATED  	/* variable number of inslots! */
  GENERATED  	/* variable number of outslots! */
  GENERATED  
  GENERATED  
#            	line 630 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_invoke);
  GENERATED  
  GENERATED  	INSTRUCTION_GET_METHODDESC(IPTR, md);
  GENERATED  	CHECK_STACK_DEPTH(md->paramslots);
  GENERATED  
  GENERATED  	if (!typecheck_stackbased_verify_invocation(STATE, stack, stackfloor))
  GENERATED  		EXCEPTION;
  GENERATED  
  GENERATED  	stack -= md->paramslots;
  GENERATED  
  GENERATED  	if (md->returntype.type != TYPE_VOID) {
  GENERATED  		if (IS_2_WORD_TYPE(md->returntype.type)) {
  GENERATED  			CHECK_STACK_SPACE(2);
  GENERATED  			stack += 2;
  GENERATED  			stack[0].type = TYPE_VOID;
  GENERATED  			stack[-1].type = md->returntype.type;
  GENERATED  		}
  GENERATED  		else {
  GENERATED  			CHECK_STACK_SPACE(1);
  GENERATED  			stack += 1;
  GENERATED  			stack[0].type = md->returntype.type;
  GENERATED  		}
  GENERATED  	}
  GENERATED  	TYPECHECK_COUNTIF(INSTRUCTION_IS_UNRESOLVED(IPTR), stat_ins_invoke_unresolved);
  GENERATED  
#            	line 1750 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ARRAYLENGTH:
  GENERATED  	/* (A--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 233 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!TYPEINFO_MAYBE_ARRAY(OP1->typeinfo)
  GENERATED  			&& OP1->typeinfo.typeclass.cls != pseudo_class_Arraystub)
  GENERATED  		VERIFY_ERROR("illegal instruction: ARRAYLENGTH on non-array");
  GENERATED  
#            	line 1772 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack[0].type = TYPE_INT;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_ATHROW:
  GENERATED  	/* (A--) */
  GENERATED  	maythrow = true;
  GENERATED  	superblockend = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 493 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_athrow);
  GENERATED  	r = typeinfo_is_assignable_to_class(&OP1->typeinfo,
  GENERATED  			CLASSREF_OR_CLASSINFO(class_java_lang_Throwable));
  GENERATED  	if (r == typecheck_FALSE)
  GENERATED  		VERIFY_ERROR("illegal instruction: ATHROW on non-Throwable");
  GENERATED  	if (r == typecheck_FAIL)
  GENERATED  		EXCEPTION;
  GENERATED  	if (r == typecheck_MAYBE) {
  GENERATED  		/* the check has to be postponed. we need a patcher */
  GENERATED  		TYPECHECK_COUNT(stat_ins_athrow_unresolved);
  GENERATED  		IPTR->sx.s23.s2.uc = create_unresolved_class(
  GENERATED  				METHOD,
  GENERATED  				/* XXX make this more efficient, use class_java_lang_Throwable
  GENERATED  				 * directly */
  GENERATED  				class_get_classref(METHOD->class,utf_java_lang_Throwable),
  GENERATED  				&OP1->typeinfo);
  GENERATED  		IPTR->flags.bits |= INS_FLAG_UNRESOLVED;
  GENERATED  	}
  GENERATED  
#            	line 1814 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack += -1;
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_CHECKCAST:
  GENERATED  	/* (A--A) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 367 "src/vm/jit/verify/icmds.c"
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
#            	line 1848 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
#            	undef DST
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_INSTANCEOF:
  GENERATED  	/* (A--I) */
  GENERATED  	maythrow = true;
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  
#            	define OP1 (&(stack[0]))
  GENERATED  
#            	define DST  (&(stack[0]))
  GENERATED  	/* may use stack[0] ... stack[0] */
  GENERATED  
  GENERATED  
#            	line 379 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* returnAddress is not allowed */
  GENERATED  	if (!TYPEINFO_IS_REFERENCE(OP1->typeinfo))
  GENERATED  		VERIFY_ERROR("Illegal instruction: INSTANCEOF on non-reference");
  GENERATED  
  GENERATED  	/* XXX should propagate type information to the following if-branches */
  GENERATED  
#            	line 1877 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	stack[0].type = TYPE_INT;
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
  GENERATED  	/* variable number of inslots! */
  GENERATED  
  GENERATED  
#            	line 668 "src/vm/jit/verify/icmds.c"
  GENERATED  	if (!typecheck_stackbased_multianewarray(STATE, stack, stackfloor))
  GENERATED  		EXCEPTION;
  GENERATED  	stack -= (IPTR->s1.argcount - 1);
  GENERATED  	stack[0].type = TYPE_ADR;
  GENERATED  
#            	line 1900 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_IFNULL:
  GENERATED  case ICMD_IFNONNULL:
  GENERATED  	/* (A--) */
  GENERATED  	CHECK_STACK_DEPTH(1);
  GENERATED  	CHECK_STACK_TYPE(stack[0], TYPE_ADR);
  GENERATED  	stack += -1;
  GENERATED  	/* CAUTION: stack pointer changed before custom code! */
  GENERATED  
#            	define OP1 (&(stack[1]))
  GENERATED  	/* may use stack[1] ... stack[1] */
  GENERATED  
  GENERATED  
#            	line 391 "src/vm/jit/verify/icmds.c"
  GENERATED  	/* {RESULTNOW} */
  GENERATED  	TYPECHECK_COUNT(stat_ins_branch);
  GENERATED  
  GENERATED  	/* propagate stack and variables to the target block */
  GENERATED  	REACH(IPTR->dst);
  GENERATED  
#            	line 1924 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#            	undef OP1
  GENERATED  
  GENERATED  
  GENERATED  case ICMD_BUILTIN:
  GENERATED  	/* (.--1|.--2|.--) */
  GENERATED  	maythrow = true;
  GENERATED  	/* variable number of inslots! */
  GENERATED  	/* variable number of outslots! */
  GENERATED  
  GENERATED  
#            	line 684 "src/vm/jit/verify/icmds.c"
  GENERATED  	TYPECHECK_COUNT(stat_ins_builtin);
  GENERATED  	if (!typecheck_stackbased_verify_builtin(STATE, stack, stackfloor))
  GENERATED  		EXCEPTION;
  GENERATED  
  GENERATED  	/* pop operands and push return value */
  GENERATED  	{
  GENERATED  		u1 rtype = IPTR->sx.s23.s3.bte->md->returntype.type;
  GENERATED  		stack -=  IPTR->sx.s23.s3.bte->md->paramslots;
  GENERATED  		if (rtype != TYPE_VOID) {
  GENERATED  			if (IS_2_WORD_TYPE(rtype))
  GENERATED  				stack += 2;
  GENERATED  			else
  GENERATED  				stack += 1;
  GENERATED  		}
  GENERATED  	}
  GENERATED  
#            	line 1956 "src/vm/jit/verify/typecheck-stackbased-gen.inc"
  GENERATED  
  GENERATED  	break;
  GENERATED  
  GENERATED  
#undef GENERATED
/* vim:filetype=c:
 */
