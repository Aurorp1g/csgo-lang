// Package vm 提供 CSGO 编程语言的虚拟机实现
//
// 本包实现了 CSGO 语言的字节码解释器，基于 CPython 的虚拟机设计。
// 支持栈帧管理、函数调用、闭包、协程等核心功能。
//
// 核心类型：
//   - Frame: 栈帧结构，表示函数调用上下文
//   - VM: 虚拟机主结构，负责字节码解释执行
//   - CodeObject: 代码对象，存储编译后的字节码
//   - Scope: 作用域，用于变量查找
//
// 功能特性：
//   - 字节码解释执行
//   - 栈帧管理（调用/返回）
//   - 局部变量和全局变量
//   - 闭包支持
//   - 协程调度集成
//
// 参考资料：
//   - CPython Objects/frameobject.c: 栈帧实现
//   - CPython Python/ceval.c: 字节码解释器
package vm

import (
	"fmt"
	"os"
)

type Frame struct {
	Code    *CodeObject
	IP      int
	Stack   []*Object
	Locals  []*Object
	Globals *Scope
	Closure []*Object
	Back    *Frame
}

func NewFrame(code *CodeObject, globals *Scope, locals []*Object) *Frame {
	stackSize := int(code.StackSize)
	if stackSize < 4 {
		stackSize = 4
	}
	return &Frame{
		Code:    code,
		IP:      0,
		Stack:   make([]*Object, 0, stackSize),
		Locals:  locals,
		Globals: globals,
		Closure: nil,
		Back:    nil,
	}
}

func (f *Frame) Push(v *Object) {
	f.Stack = append(f.Stack, v)
}

func (f *Frame) Pop() *Object {
	if len(f.Stack) == 0 {
		return NewNone()
	}
	v := f.Stack[len(f.Stack)-1]
	f.Stack = f.Stack[:len(f.Stack)-1]
	return v
}

func (f *Frame) Top() *Object {
	if len(f.Stack) == 0 {
		return NewNone()
	}
	return f.Stack[len(f.Stack)-1]
}

func (f *Frame) Peek(n int) *Object {
	if len(f.Stack) <= n {
		return NewNone()
	}
	return f.Stack[len(f.Stack)-1-n]
}

func (f *Frame) SetTop(v *Object) {
	if len(f.Stack) > 0 {
		f.Stack[len(f.Stack)-1] = v
	}
}

func (f *Frame) Drop(n int) {
	if len(f.Stack) >= n {
		f.Stack = f.Stack[:len(f.Stack)-n]
	}
}

func (f *Frame) rot(n int) {
	l := len(f.Stack)
	if l < n {
		return
	}
	top := f.Stack[l-1]
	copy(f.Stack[l-n+1:], f.Stack[l-n:l-1])
	f.Stack[l-n] = top
}

type Scope struct {
	Table    map[string]*Object
	Outer    *Scope
	Builtins map[string]*Object
}

func NewScope(outer *Scope) *Scope {
	return &Scope{
		Table:    make(map[string]*Object),
		Outer:    outer,
		Builtins: make(map[string]*Object),
	}
}

func (s *Scope) Get(name string) *Object {
	if v, ok := s.Table[name]; ok {
		return v
	}
	if s.Outer != nil {
		return s.Outer.Get(name)
	}
	if v, ok := s.Builtins[name]; ok {
		return v
	}
	return NewNone()
}

func (s *Scope) Set(name string, v *Object) {
	s.Table[name] = v
}

func (s *Scope) Define(name string, v *Object) {
	s.Table[name] = v
}

func (s *Scope) Has(name string) bool {
	_, ok := s.Table[name]
	return ok
}

type VM struct {
	Frames   []*Frame
	Globals  *Scope
	Modules  map[string]*CodeObject
	Exited   bool
	ExitCode int
	Stdout   *os.File
	Stderr   *os.File
	Debug    bool
}

func NewVM() *VM {
	stdlib := NewScope(nil)
	registerBuiltins(stdlib)

	return &VM{
		Frames:   make([]*Frame, 0),
		Globals:  stdlib,
		Modules:  make(map[string]*CodeObject),
		Exited:   false,
		ExitCode: 0,
		Stdout:   os.Stdout,
		Stderr:   os.Stderr,
		Debug:    false,
	}
}

func (vm *VM) PushFrame(frame *Frame) {
	vm.Frames = append(vm.Frames, frame)
}

func (vm *VM) PopFrame() *Frame {
	if len(vm.Frames) == 0 {
		return nil
	}
	frame := vm.Frames[len(vm.Frames)-1]
	vm.Frames = vm.Frames[:len(vm.Frames)-1]
	return frame
}

func (vm *VM) CurrentFrame() *Frame {
	if len(vm.Frames) == 0 {
		return nil
	}
	return vm.Frames[len(vm.Frames)-1]
}

func (vm *VM) Run(code *CodeObject) error {
	vm.Modules[code.Filename] = code

	frame := NewFrame(code, vm.Globals, make([]*Object, code.NLocals))
	vm.PushFrame(frame)

	if vm.Debug {
		fmt.Fprintf(vm.Stdout, "=== Running code: %s ===\n", code.Name)
	}

	err := vm.Execute()
	if err != nil {
		return err
	}

	return nil
}

func (vm *VM) Execute() error {
	for !vm.Exited {
		frame := vm.CurrentFrame()
		if frame == nil {
			break
		}

		if frame.IP >= len(frame.Code.Instructions) {
			return fmt.Errorf("IP out of bounds: %d >= %d", frame.IP, len(frame.Code.Instructions))
		}

		opcode := Opcode(frame.Code.Instructions[frame.IP])
		frame.IP++

		if vm.Debug {
			vm.dumpInstruction(frame, opcode)
		}

		err := vm.dispatch(frame, opcode)
		if err != nil {
			return vm.handleError(frame, err)
		}
	}

	return nil
}

func (vm *VM) dispatch(frame *Frame, opcode Opcode) error {
	var oparg uint16

	if HasArg[opcode] {
		if frame.IP+1 > len(frame.Code.Instructions) {
			return fmt.Errorf("unexpected end of instructions")
		}
		oparg = uint16(frame.Code.Instructions[frame.IP]) | uint16(frame.Code.Instructions[frame.IP+1])<<8
		frame.IP += 2
	}

	switch opcode {
	case OpNOP:
		return nil

	case OpLOAD_CONST:
		if int(oparg) >= len(frame.Code.Constants) {
			return fmt.Errorf("LOAD_CONST: index out of range: %d", oparg)
		}
		frame.Push(frame.Code.Constants[oparg])

	case OpLOAD_FAST:
		if int(oparg) >= len(frame.Locals) {
			return fmt.Errorf("LOAD_FAST: index out of range: %d", oparg)
		}
		frame.Push(frame.Locals[oparg])

	case OpLOAD_GLOBAL:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("LOAD_GLOBAL: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		val := frame.Globals.Get(name)
		if val.Type == TypeNone && !frame.Globals.Has(name) {
			return fmt.Errorf("name %q is not defined", name)
		}
		frame.Push(val)

	case OpLOAD_NAME:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("LOAD_NAME: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		val := frame.Globals.Get(name)
		frame.Push(val)

	case OpSTORE_FAST:
		if int(oparg) >= len(frame.Locals) {
			frame.Locals = append(frame.Locals, make([]*Object, int(oparg)-len(frame.Locals)+1)...)
		}
		frame.Locals[oparg] = frame.Pop()

	case OpSTORE_GLOBAL:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("STORE_GLOBAL: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		frame.Globals.Define(name, frame.Pop())

	case OpSTORE_NAME:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("STORE_NAME: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		frame.Globals.Define(name, frame.Pop())

	case OpBINARY_ADD:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "+")
		frame.Push(result)

	case OpBINARY_SUBTRACT:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "-")
		frame.Push(result)

	case OpBINARY_MULTIPLY:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "*")
		frame.Push(result)

	case OpBINARY_TRUE_DIVIDE:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "/")
		frame.Push(result)

	case OpBINARY_FLOOR_DIVIDE:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "//")
		frame.Push(result)

	case OpBINARY_MODULO:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "%")
		frame.Push(result)

	case OpBINARY_POWER:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "**")
		frame.Push(result)

	case OpBINARY_LSHIFT:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "<<")
		frame.Push(result)

	case OpBINARY_RSHIFT:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, ">>")
		frame.Push(result)

	case OpBINARY_AND:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "&")
		frame.Push(result)

	case OpBINARY_OR:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "|")
		frame.Push(result)

	case OpBINARY_XOR:
		b := frame.Pop()
		a := frame.Pop()
		result := binaryOp(a, b, "^")
		frame.Push(result)

	case OpUNARY_POSITIVE:
		a := frame.Pop()
		result := unaryOp(a, "+")
		frame.Push(result)

	case OpUNARY_NEGATIVE:
		a := frame.Pop()
		result := unaryOp(a, "-")
		frame.Push(result)

	case OpUNARY_NOT:
		a := frame.Pop()
		if isTruthy(a) {
			frame.Push(NewBool(false))
		} else {
			frame.Push(NewBool(true))
		}

	case OpUNARY_INVERT:
		a := frame.Pop()
		result := unaryOp(a, "~")
		frame.Push(result)

	case OpCOMPARE_OP:
		b := frame.Pop()
		a := frame.Pop()
		result := Compare(CompareOp(oparg), a, b)
		frame.Push(result)

	case OpPOP_JUMP_IF_FALSE:
		cond := frame.Pop()
		if !isTruthy(cond) {
			frame.IP = int(oparg)
		}

	case OpPOP_JUMP_IF_TRUE:
		cond := frame.Pop()
		if isTruthy(cond) {
			frame.IP = int(oparg)
		}

	case OpJUMP_ABSOLUTE:
		frame.IP = int(oparg)

	case OpJUMP_FORWARD:
		frame.IP += int(oparg)

	case OpRETURN_VALUE:
		retVal := frame.Pop()
		vm.PopFrame()
		if vm.CurrentFrame() != nil {
			vm.CurrentFrame().Push(retVal)
		}

	case OpCALL_FUNCTION:
		nArgs := int(oparg)
		args := make([]*Object, nArgs)
		for i := nArgs - 1; i >= 0; i-- {
			args[i] = frame.Pop()
		}
		fn := frame.Pop()
		return vm.callFunction(frame, fn, args)

	case OpCALL_METHOD:
		nArgs := int(oparg)
		args := make([]*Object, nArgs)
		for i := nArgs - 1; i >= 0; i-- {
			args[i] = frame.Pop()
		}
		obj := frame.Pop()
		method := frame.Pop()
		return vm.callMethod(frame, obj, method, args)

	case OpLOAD_METHOD:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("LOAD_METHOD: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		obj := frame.Pop()
		method := vm.getAttribute(obj, name)
		frame.Push(obj)
		frame.Push(method)

	case OpPOP_TOP:
		frame.Pop()

	case OpROT_TWO:
		frame.rot(2)

	case OpROT_THREE:
		frame.rot(3)

	case OpROT_FOUR:
		frame.rot(4)

	case OpDUP_TOP:
		top := frame.Top()
		frame.Push(top)

	case OpDUP_TOP_TWO:
		a := frame.Peek(0)
		b := frame.Peek(1)
		frame.Push(b)
		frame.Push(a)

	case OpBUILD_LIST:
		n := int(oparg)
		items := make([]*Object, n)
		for i := n - 1; i >= 0; i-- {
			items[i] = frame.Pop()
		}
		list := &Object{Type: TypeList, Value: items}
		frame.Push(list)

	case OpBUILD_TUPLE:
		n := int(oparg)
		items := make([]*Object, n)
		for i := n - 1; i >= 0; i-- {
			items[i] = frame.Pop()
		}
		tuple := &Object{Type: TypeTuple, Value: items}
		frame.Push(tuple)

	case OpBUILD_MAP:
		n := int(oparg)
		dict := NewDict()
		m := dict.Value.(map[*Object]*Object)
		for i := 0; i < n; i++ {
			value := frame.Pop()
			key := frame.Pop()
			m[key] = value
		}
		frame.Push(dict)

	case OpBUILD_SET:
		n := int(oparg)
		set := NewList()
		items := set.Value.([]*Object)
		for i := 0; i < n; i++ {
			v := frame.Pop()
			found := false
			for _, existing := range items {
				if equal(existing, v) {
					found = true
					break
				}
			}
			if !found {
				items = append(items, v)
			}
		}
		set.Value = items
		frame.Push(set)

	case OpBUILD_SLICE:
		n := int(oparg)
		var start, stop, step *Object
		switch n {
		case 1:
			stop = frame.Pop()
		case 2:
			stop = frame.Pop()
			start = frame.Pop()
		case 3:
			step = frame.Pop()
			stop = frame.Pop()
			start = frame.Pop()
		}
		slice := &Object{Type: TypeList, Value: []*Object{start, stop, step}}
		frame.Push(slice)

	case OpUNPACK_SEQUENCE:
		seq := frame.Pop()
		n := int(oparg)
		unpacked, err := unpackSequence(seq, n)
		if err != nil {
			return err
		}
		for _, v := range unpacked {
			frame.Push(v)
		}

	case OpGET_ITER:
		obj := frame.Pop()
		iter := getIterator(obj)
		frame.Push(iter)

	case OpFOR_ITER:
		iter := frame.Top()
		next, err := iteratorNext(iter)
		if err != nil {
			return err
		}
		if next == nil {
			frame.IP = int(oparg)
		} else {
			frame.Push(next)
		}

	case OpBREAK_LOOP:
		return fmt.Errorf("BREAK_LOOP not implemented")

	case OpCONTINUE_LOOP:
		return fmt.Errorf("CONTINUE_LOOP not implemented")

	case OpMAKE_FUNCTION:
		codeIdx := oparg
		if int(codeIdx) >= len(frame.Code.Constants) {
			return fmt.Errorf("MAKE_FUNCTION: code index out of range: %d", codeIdx)
		}
		codeObj := frame.Code.Constants[codeIdx]
		if codeObj.Type != TypeString {
			return fmt.Errorf("MAKE_FUNCTION: expected string constant for code object")
		}
		fn := NewFunction(codeObj.Value.(string), vm.Modules[codeObj.Value.(string)])
		frame.Push(fn)

	case OpLIST_ADD:
		value := frame.Pop()
		list := frame.Top()
		if list.Type == TypeList {
			list.Value = append(list.Value.([]*Object), value)
		}

	case OpSET_ADD:
		value := frame.Pop()
		set := frame.Top()
		if set.Type == TypeList {
			items := set.Value.([]*Object)
			found := false
			for _, existing := range items {
				if equal(existing, value) {
					found = true
					break
				}
			}
			if !found {
				items = append(items, value)
			}
			set.Value = items
		}

	case OpDELETE_FAST:
		if int(oparg) < len(frame.Locals) {
			frame.Locals[oparg] = NewNone()
		}

	case OpDELETE_GLOBAL:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("DELETE_GLOBAL: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		delete(frame.Globals.Table, name)

	case OpDELETE_NAME:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("DELETE_NAME: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		delete(frame.Globals.Table, name)

	case OpLOAD_SUBSCR:
		key := frame.Pop()
		obj := frame.Pop()
		result := subscriptGet(obj, key)
		frame.Push(result)

	case OpSTORE_SUBSCR:
		value := frame.Pop()
		key := frame.Pop()
		obj := frame.Pop()
		subscriptSet(obj, key, value)

	case OpLOAD_ATTR:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("LOAD_ATTR: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		obj := frame.Pop()
		result := vm.getAttribute(obj, name)
		frame.Push(result)

	case OpSTORE_ATTR:
		if int(oparg) >= len(frame.Code.Names) {
			return fmt.Errorf("STORE_ATTR: index out of range: %d", oparg)
		}
		name := frame.Code.Names[oparg]
		value := frame.Pop()
		obj := frame.Pop()
		vm.setAttribute(obj, name, value)

	case OpRAISE_VARARGS:
		return fmt.Errorf("RAISE_VARARGS not fully implemented")

	case OpSETUP_EXCEPT:
		frame.IP += int(oparg)

	case OpSETUP_FINALLY:
		frame.IP += int(oparg)

	case OpEND_FINALLY:
		frame.Pop()

	case OpIMPORT_NAME:
		return fmt.Errorf("IMPORT_NAME not implemented")

	case OpIMPORT_FROM:
		return fmt.Errorf("IMPORT_FROM not implemented")

	case OpIMPORT_STAR:
		return fmt.Errorf("IMPORT_STAR not implemented")

	default:
		return fmt.Errorf("unknown opcode: %d (%s)", opcode, opcode)
	}

	return nil
}

func (vm *VM) callFunction(frame *Frame, fn *Object, args []*Object) error {
	switch fn.Type {
	case TypeBuiltin:
		fnBuiltin := fn.Value.(BuiltinFunc)
		result, err := fnBuiltin(vm, args)
		if err != nil {
			return err
		}
		frame.Push(result)
		return nil

	case TypeFunction:
		fnFunc := fn.Value.(*Function)
		code := fnFunc.Code

		if code == nil {
			return fmt.Errorf("function %q has no code object", fnFunc.Name)
		}

		newFrame := NewFrame(code, frame.Globals, make([]*Object, code.NLocals))
		newFrame.Back = frame
		newFrame.Closure = fnFunc.Closure

		nParams := int(code.ArgCount) + int(code.KwOnlyArgCount)
		for i := 0; i < nParams && i < len(args); i++ {
			newFrame.Locals[i] = args[i]
		}

		if len(args) > nParams {
			extraArgs := args[nParams:]
			newFrame.Locals = append(newFrame.Locals, extraArgs...)
		}

		vm.PushFrame(newFrame)
		return nil

	default:
		return fmt.Errorf("cannot call %s", objectTypeToString(fn.Type))
	}
}

func (vm *VM) callMethod(frame *Frame, obj, method *Object, args []*Object) error {
	if method.Type == TypeBuiltin {
		args = append([]*Object{obj}, args...)
		fnBuiltin := method.Value.(BuiltinFunc)
		result, err := fnBuiltin(vm, args)
		if err != nil {
			return err
		}
		frame.Push(result)
		return nil
	}
	return vm.callFunction(frame, method, args)
}

func (vm *VM) getAttribute(obj *Object, name string) *Object {
	switch obj.Type {
	case TypeDict:
		m := obj.Value.(map[*Object]*Object)
		key := NewString(name)
		if v, ok := m[key]; ok {
			return v
		}
		return NewNone()
	case TypeList:
		if name == "append" {
			return NewBuiltin(func(vm *VM, args []*Object) (*Object, error) {
				if len(args) < 2 {
					return nil, fmt.Errorf("append requires at least 1 argument")
				}
				list := args[0]
				value := args[1]
				if list.Type == TypeList {
					list.Value = append(list.Value.([]*Object), value)
				}
				return NewNone(), nil
			})
		}
		if name == "len" {
			return NewBuiltin(lenBuiltin)
		}
	}
	return NewNone()
}

func (vm *VM) setAttribute(obj *Object, name string, value *Object) error {
	switch obj.Type {
	case TypeDict:
		m := obj.Value.(map[*Object]*Object)
		key := NewString(name)
		m[key] = value
		return nil
	}
	return fmt.Errorf("cannot set attribute on %s", objectTypeToString(obj.Type))
}

func (vm *VM) handleError(frame *Frame, err error) error {
	fmt.Fprintf(vm.Stderr, "Error: %v\n", err)
	if vm.Debug {
		vm.dumpStack(frame)
	}
	vm.ExitCode = 1
	vm.Exited = true
	return err
}

func (vm *VM) dumpInstruction(frame *Frame, opcode Opcode) {
	fmt.Fprintf(vm.Stdout, "IP: %d, Opcode: %s", frame.IP-1, opcode)
	if HasArg[opcode] {
		ip := frame.IP
		if ip+1 < len(frame.Code.Instructions) {
			oparg := uint16(frame.Code.Instructions[ip]) | uint16(frame.Code.Instructions[ip+1])<<8
			fmt.Fprintf(vm.Stdout, " (arg=%d)", oparg)
		}
	}
	fmt.Fprintf(vm.Stdout, ", Stack: %d items\n", len(frame.Stack))
}

func (vm *VM) dumpStack(frame *Frame) {
	fmt.Fprintf(vm.Stdout, "Stack (%d items):\n", len(frame.Stack))
	for i, v := range frame.Stack {
		fmt.Fprintf(vm.Stdout, "  [%d] %v\n", i, v)
	}
}
