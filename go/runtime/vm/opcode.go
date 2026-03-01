package vm

type Opcode uint8

const (
	OpNOP Opcode = iota

	OpLOAD_CONST
	OpLOAD_FAST
	OpLOAD_GLOBAL
	OpLOAD_NAME
	OpLOAD_ATTR
	OpLOAD_SUPER_ATTR
	OpLOAD_DEREF
	OpLOAD_CLOSURE
	OpLOAD_SUBSCR

	OpSTORE_FAST
	OpSTORE_GLOBAL
	OpSTORE_NAME
	OpSTORE_ATTR
	OpSTORE_DEREF
	OpSTORE_SUBSCR

	OpDELETE_FAST
	OpDELETE_GLOBAL
	OpDELETE_NAME
	OpDELETE_ATTR
	OpDELETE_DEREF
	OpDELETE_SUBSCR

	OpBINARY_ADD
	OpBINARY_SUBTRACT
	OpBINARY_MULTIPLY
	OpBINARY_TRUE_DIVIDE
	OpBINARY_FLOOR_DIVIDE
	OpBINARY_MODULO
	OpBINARY_POWER
	OpBINARY_LSHIFT
	OpBINARY_RSHIFT
	OpBINARY_AND
	OpBINARY_OR
	OpBINARY_XOR
	OpBINARY_MATRIX_MULTIPLY

	OpUNARY_POSITIVE
	OpUNARY_NEGATIVE
	OpUNARY_NOT
	OpUNARY_INVERT

	OpCOMPARE_OP

	OpPOP_JUMP_IF_TRUE
	OpPOP_JUMP_IF_FALSE
	OpJUMP_IF_TRUE_OR_POP
	OpJUMP_IF_FALSE_OR_POP
	OpJUMP_ABSOLUTE
	OpJUMP_FORWARD

	OpBREAK_LOOP
	OpCONTINUE_LOOP

	OpRETURN_VALUE
	OpYIELD_VALUE
	OpYIELD_FROM

	OpPOP_TOP
	OpROT_TWO
	OpROT_THREE
	OpROT_FOUR
	OpDUP_TOP
	OpDUP_TOP_TWO

	OpCALL_FUNCTION
	OpCALL_FUNCTION_KW
	OpCALL_FUNCTION_EX
	OpCALL_METHOD
	OpMAKE_FUNCTION
	OpLOAD_METHOD

	OpBUILD_TUPLE
	OpBUILD_LIST
	OpBUILD_SET
	OpBUILD_MAP
	OpBUILD_STRING
	OpBUILD_TUPLE_UNPACK
	OpBUILD_LIST_UNPACK
	OpBUILD_SET_UNPACK
	OpBUILD_MAP_UNPACK
	OpBUILD_MAP_UNPACK_WITH_CALL
	OpBUILD_CONST_KEY_MAP
	OpBUILD_SLICE

	OpUNPACK_SEQUENCE
	OpUNPACK_EX

	OpGET_ITER
	OpFOR_ITER
	OpGET_YIELD_FROM_ITER

	OpGET_AWAITABLE
	OpGET_AITER
	OpGET_ANEXT
	OpBEFORE_ASYNC_WITH
	OpSETUP_ASYNC_WITH
	OpEND_ASYNC_FOR
	OpSETUP_CLEANUP
	OpEND_CLEANUP

	OpRAISE_VARARGS
	OpSETUP_EXCEPT
	OpSETUP_FINALLY
	OpSETUP_WITH
	OpEND_FINALLY
	OpPOP_FINALLY
	OpBEGIN_FINALLY
	OpCALL_FINALLY

	OpWITH_CLEANUP_START
	OpWITH_CLEANUP_FINISH

	OpFORMAT_VALUE

	OpIMPORT_NAME
	OpIMPORT_FROM
	OpIMPORT_STAR

	OpLOAD_CLASSDEREF

	OpEXTENDED_ARG

	OpSET_ADD
	OpLIST_ADD

	OpBINARY_OP

	OpSEND
	OpTHROW
	OpCLOSE

	OpASYNC_GENERATOR_WRAP

	OpINSTRUCTION_COUNT
)

func (op Opcode) String() string {
	names := []string{
		"NOP", "LOAD_CONST", "LOAD_FAST", "LOAD_GLOBAL", "LOAD_NAME",
		"LOAD_ATTR", "LOAD_SUPER_ATTR", "LOAD_DEREF", "LOAD_CLOSURE", "LOAD_SUBSCR",
		"STORE_FAST", "STORE_GLOBAL", "STORE_NAME", "STORE_ATTR", "STORE_DEREF", "STORE_SUBSCR",
		"DELETE_FAST", "DELETE_GLOBAL", "DELETE_NAME", "DELETE_ATTR", "DELETE_DEREF", "DELETE_SUBSCR",
		"BINARY_ADD", "BINARY_SUBTRACT", "BINARY_MULTIPLY", "BINARY_TRUE_DIVIDE",
		"BINARY_FLOOR_DIVIDE", "BINARY_MODULO", "BINARY_POWER", "BINARY_LSHIFT",
		"BINARY_RSHIFT", "BINARY_AND", "BINARY_OR", "BINARY_XOR", "BINARY_MATRIX_MULTIPLY",
		"UNARY_POSITIVE", "UNARY_NEGATIVE", "UNARY_NOT", "UNARY_INVERT",
		"COMPARE_OP",
		"POP_JUMP_IF_TRUE", "POP_JUMP_IF_FALSE", "JUMP_IF_TRUE_OR_POP", "JUMP_IF_FALSE_OR_POP",
		"JUMP_ABSOLUTE", "JUMP_FORWARD",
		"BREAK_LOOP", "CONTINUE_LOOP",
		"RETURN_VALUE", "YIELD_VALUE", "YIELD_FROM",
		"POP_TOP", "ROT_TWO", "ROT_THREE", "ROT_FOUR", "DUP_TOP", "DUP_TOP_TWO",
		"CALL_FUNCTION", "CALL_FUNCTION_KW", "CALL_FUNCTION_EX", "CALL_METHOD",
		"MAKE_FUNCTION", "LOAD_METHOD",
		"BUILD_TUPLE", "BUILD_LIST", "BUILD_SET", "BUILD_MAP", "BUILD_STRING",
		"BUILD_TUPLE_UNPACK", "BUILD_LIST_UNPACK", "BUILD_SET_UNPACK", "BUILD_MAP_UNPACK",
		"BUILD_MAP_UNPACK_WITH_CALL", "BUILD_CONST_KEY_MAP", "BUILD_SLICE",
		"UNPACK_SEQUENCE", "UNPACK_EX",
		"GET_ITER", "FOR_ITER", "GET_YIELD_FROM_ITER",
		"GET_AWAITABLE", "GET_AITER", "GET_ANEXT", "BEFORE_ASYNC_WITH",
		"SETUP_ASYNC_WITH", "END_ASYNC_FOR", "SETUP_CLEANUP", "END_CLEANUP",
		"RAISE_VARARGS", "SETUP_EXCEPT", "SETUP_FINALLY", "SETUP_WITH",
		"END_FINALLY", "POP_FINALLY", "BEGIN_FINALLY", "CALL_FINALLY",
		"WITH_CLEANUP_START", "WITH_CLEANUP_FINISH",
		"FORMAT_VALUE",
		"IMPORT_NAME", "IMPORT_FROM", "IMPORT_STAR",
		"LOAD_CLASSDEREF",
		"EXTENDED_ARG",
		"SET_ADD", "LIST_ADD",
		"BINARY_OP",
		"SEND", "THROW", "CLOSE",
		"ASYNC_GENERATOR_WRAP",
		"INSTRUCTION_COUNT",
	}
	if op < Opcode(len(names)) {
		return names[op]
	}
	return "UNKNOWN"
}

var HasArg = map[Opcode]bool{
	OpNOP:                        false,
	OpLOAD_CONST:                 true,
	OpLOAD_FAST:                  true,
	OpLOAD_GLOBAL:                true,
	OpLOAD_NAME:                  true,
	OpLOAD_ATTR:                  true,
	OpLOAD_SUPER_ATTR:            true,
	OpLOAD_DEREF:                 true,
	OpLOAD_CLOSURE:               true,
	OpLOAD_SUBSCR:                false,
	OpSTORE_FAST:                 true,
	OpSTORE_GLOBAL:               true,
	OpSTORE_NAME:                 true,
	OpSTORE_ATTR:                 true,
	OpSTORE_DEREF:                true,
	OpSTORE_SUBSCR:               false,
	OpDELETE_FAST:                true,
	OpDELETE_GLOBAL:              true,
	OpDELETE_NAME:                true,
	OpDELETE_ATTR:                true,
	OpDELETE_DEREF:               true,
	OpDELETE_SUBSCR:              false,
	OpBINARY_ADD:                 false,
	OpBINARY_SUBTRACT:            false,
	OpBINARY_MULTIPLY:            false,
	OpBINARY_TRUE_DIVIDE:         false,
	OpBINARY_FLOOR_DIVIDE:        false,
	OpBINARY_MODULO:              false,
	OpBINARY_POWER:               false,
	OpBINARY_LSHIFT:              false,
	OpBINARY_RSHIFT:              false,
	OpBINARY_AND:                 false,
	OpBINARY_OR:                  false,
	OpBINARY_XOR:                 false,
	OpBINARY_MATRIX_MULTIPLY:     false,
	OpUNARY_POSITIVE:             false,
	OpUNARY_NEGATIVE:             false,
	OpUNARY_NOT:                  false,
	OpUNARY_INVERT:               false,
	OpCOMPARE_OP:                 true,
	OpPOP_JUMP_IF_TRUE:           true,
	OpPOP_JUMP_IF_FALSE:          true,
	OpJUMP_IF_TRUE_OR_POP:        true,
	OpJUMP_IF_FALSE_OR_POP:       true,
	OpJUMP_ABSOLUTE:              true,
	OpJUMP_FORWARD:               true,
	OpBREAK_LOOP:                 false,
	OpCONTINUE_LOOP:              false,
	OpRETURN_VALUE:               false,
	OpYIELD_VALUE:                false,
	OpYIELD_FROM:                 false,
	OpPOP_TOP:                    false,
	OpROT_TWO:                    false,
	OpROT_THREE:                  false,
	OpROT_FOUR:                   false,
	OpDUP_TOP:                    false,
	OpDUP_TOP_TWO:                false,
	OpCALL_FUNCTION:              true,
	OpCALL_FUNCTION_KW:           true,
	OpCALL_FUNCTION_EX:           false,
	OpCALL_METHOD:                false,
	OpMAKE_FUNCTION:              true,
	OpLOAD_METHOD:                true,
	OpBUILD_TUPLE:                true,
	OpBUILD_LIST:                 true,
	OpBUILD_SET:                  true,
	OpBUILD_MAP:                  true,
	OpBUILD_STRING:               true,
	OpBUILD_TUPLE_UNPACK:         true,
	OpBUILD_LIST_UNPACK:          true,
	OpBUILD_SET_UNPACK:           true,
	OpBUILD_MAP_UNPACK:           true,
	OpBUILD_MAP_UNPACK_WITH_CALL: true,
	OpBUILD_CONST_KEY_MAP:        true,
	OpBUILD_SLICE:                true,
	OpUNPACK_SEQUENCE:            true,
	OpUNPACK_EX:                  true,
	OpGET_ITER:                   false,
	OpFOR_ITER:                   true,
	OpGET_YIELD_FROM_ITER:        false,
	OpGET_AWAITABLE:              false,
	OpGET_AITER:                  false,
	OpGET_ANEXT:                  false,
	OpBEFORE_ASYNC_WITH:          false,
	OpSETUP_ASYNC_WITH:           true,
	OpEND_ASYNC_FOR:              false,
	OpSETUP_CLEANUP:              true,
	OpEND_CLEANUP:                false,
	OpRAISE_VARARGS:              true,
	OpSETUP_EXCEPT:               true,
	OpSETUP_FINALLY:              true,
	OpSETUP_WITH:                 true,
	OpEND_FINALLY:                false,
	OpPOP_FINALLY:                true,
	OpBEGIN_FINALLY:              true,
	OpCALL_FINALLY:               true,
	OpWITH_CLEANUP_START:         false,
	OpWITH_CLEANUP_FINISH:        false,
	OpFORMAT_VALUE:               true,
	OpIMPORT_NAME:                true,
	OpIMPORT_FROM:                true,
	OpIMPORT_STAR:                false,
	OpLOAD_CLASSDEREF:            true,
	OpEXTENDED_ARG:               true,
	OpSET_ADD:                    true,
	OpLIST_ADD:                   true,
	OpBINARY_OP:                  true,
	OpSEND:                       true,
	OpTHROW:                      true,
	OpCLOSE:                      false,
	OpASYNC_GENERATOR_WRAP:       false,
}
