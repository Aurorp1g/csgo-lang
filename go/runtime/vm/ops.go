// Package vm 提供 CSGO 编程语言的虚拟机实现
//
// 本包实现了 CSGO 语言的字节码操作实现
package vm

import (
	"fmt"
	"math"
	"strconv"
	"strings"
)

// isTruthy 判断对象是否为真值
//
// 用于条件判断和逻辑运算。根据对象类型判断真值：
//   - None: 返回 false
//   - Bool: 返回布尔值本身
//   - Int/Float: 非零为 true
//   - String/List/Dict/Tuple: 非空为 true
func isTruthy(obj *Object) bool {
	switch obj.Type {
	case TypeNone:
		return false
	case TypeBool:
		return obj.Value.(bool)
	case TypeInt:
		return obj.Value.(int64) != 0
	case TypeFloat:
		return obj.Value.(float64) != 0
	case TypeString:
		return len(obj.Value.(string)) > 0
	case TypeList:
		return len(obj.Value.([]*Object)) > 0
	case TypeDict:
		return len(obj.Value.(map[*Object]*Object)) > 0
	case TypeTuple:
		return len(obj.Value.([]*Object)) > 0
	}
	return true
}

func binaryOp(a, b *Object, op string) *Object {
	aInt, aFloat := getNumeric(a)
	bInt, bFloat := getNumeric(b)

	if op == "+" {
		if a.Type == TypeString && b.Type == TypeString {
			return NewString(a.Value.(string) + b.Value.(string))
		}
		if aInt != nil && bInt != nil {
			return NewInt(aInt.Value.(int64) + bInt.Value.(int64))
		}
		if (aInt != nil || aFloat != nil) && (bInt != nil || bFloat != nil) {
			af := toFloat(a)
			bf := toFloat(b)
			return NewFloat(af + bf)
		}
		if a.Type == TypeList && b.Type == TypeList {
			la := a.Value.([]*Object)
			lb := b.Value.([]*Object)
			result := make([]*Object, len(la)+len(lb))
			copy(result, la)
			copy(result[len(la):], lb)
			return &Object{Type: TypeList, Value: result}
		}
	}

	if aInt != nil && bInt != nil {
		av := aInt.Value.(int64)
		bv := bInt.Value.(int64)
		switch op {
		case "-":
			return NewInt(av - bv)
		case "*":
			return NewInt(av * bv)
		case "/":
			if bv == 0 {
				return NewFloat(math.Inf(1))
			}
			return NewFloat(float64(av) / float64(bv))
		case "//":
			if bv == 0 {
				return NewInt(0)
			}
			return NewInt(av / bv)
		case "%":
			if bv == 0 {
				return NewInt(0)
			}
			return NewInt(av % bv)
		case "**":
			return NewInt(int64(math.Pow(float64(av), float64(bv))))
		case "<<":
			return NewInt(av << uint(bv))
		case ">>":
			return NewInt(av >> uint(bv))
		case "&":
			return NewInt(av & bv)
		case "|":
			return NewInt(av | bv)
		case "^":
			return NewInt(av ^ bv)
		}
	}

	if aFloat != nil || bFloat != nil {
		af := toFloat(a)
		bf := toFloat(b)
		switch op {
		case "-":
			return NewFloat(af - bf)
		case "*":
			return NewFloat(af * bf)
		case "/":
			return NewFloat(af / bf)
		case "//":
			return NewFloat(math.Floor(af / bf))
		case "%":
			return NewFloat(math.Mod(af, bf))
		case "**":
			return NewFloat(math.Pow(af, bf))
		}
	}

	return NewNone()
}

func unaryOp(a *Object, op string) *Object {
	switch op {
	case "+":
		if a.Type == TypeInt {
			return a
		}
		if a.Type == TypeFloat {
			return a
		}
		if a.Type == TypeInt {
			return a
		}
	case "-":
		if a.Type == TypeInt {
			return NewInt(-a.Value.(int64))
		}
		if a.Type == TypeFloat {
			return NewFloat(-a.Value.(float64))
		}
	case "~":
		if a.Type == TypeInt {
			return NewInt(^a.Value.(int64))
		}
	}
	return NewNone()
}

func getNumeric(obj *Object) (*Object, *Object) {
	if obj.Type == TypeInt {
		return obj, nil
	}
	if obj.Type == TypeFloat {
		return nil, obj
	}
	return nil, nil
}

func toFloat(obj *Object) float64 {
	switch obj.Type {
	case TypeInt:
		return float64(obj.Value.(int64))
	case TypeFloat:
		return obj.Value.(float64)
	}
	return 0
}

func toInt(obj *Object) int64 {
	switch obj.Type {
	case TypeInt:
		return obj.Value.(int64)
	case TypeFloat:
		return int64(obj.Value.(float64))
	case TypeString:
		if v, err := strconv.ParseInt(obj.Value.(string), 10, 64); err == nil {
			return v
		}
	}
	return 0
}

func subscriptGet(obj, key *Object) *Object {
	switch obj.Type {
	case TypeList:
		list := obj.Value.([]*Object)
		idx := toInt(key)
		if idx < 0 {
			idx = int64(len(list)) + idx
		}
		if idx < 0 || idx >= int64(len(list)) {
			return NewNone()
		}
		return list[idx]
	case TypeTuple:
		tuple := obj.Value.([]*Object)
		idx := toInt(key)
		if idx < 0 {
			idx = int64(len(tuple)) + idx
		}
		if idx < 0 || idx >= int64(len(tuple)) {
			return NewNone()
		}
		return tuple[idx]
	case TypeString:
		s := obj.Value.(string)
		idx := toInt(key)
		if idx < 0 {
			idx = int64(len(s)) + idx
		}
		if idx < 0 || idx >= int64(len(s)) {
			return NewNone()
		}
		return NewString(string(s[idx]))
	case TypeDict:
		m := obj.Value.(map[*Object]*Object)
		if v, ok := m[key]; ok {
			return v
		}
		return NewNone()
	}
	return NewNone()
}

func objectTypeToString(objType ObjectType) string {
	switch objType {
	case TypeNone:
		return "NoneType"
	case TypeBool:
		return "bool"
	case TypeInt:
		return "int"
	case TypeFloat:
		return "float"
	case TypeString:
		return "str"
	case TypeList:
		return "list"
	case TypeDict:
		return "dict"
	case TypeFunction:
		return "function"
	case TypeBuiltin:
		return "builtin_function_or_method"
	case TypeTuple:
		return "tuple"
	case TypeRange:
		return "range"
	case TypeCell:
		return "cell"
	default:
		return "unknown"
	}
}

func subscriptSet(obj, key, value *Object) error {
	switch obj.Type {
	case TypeList:
		list := obj.Value.([]*Object)
		idx := toInt(key)
		if idx < 0 {
			idx = int64(len(list)) + idx
		}
		if idx < 0 || idx >= int64(len(list)) {
			return fmt.Errorf("list index out of range")
		}
		list[idx] = value
		return nil
	case TypeDict:
		m := obj.Value.(map[*Object]*Object)
		m[key] = value
		return nil
	}
	return fmt.Errorf("cannot subscript %s", objectTypeToString(obj.Type))
}

func unpackSequence(seq *Object, n int) ([]*Object, error) {
	switch seq.Type {
	case TypeList:
		list := seq.Value.([]*Object)
		if len(list) != n {
			return nil, fmt.Errorf("unpack sequence: expected %d items, got %d", n, len(list))
		}
		return list, nil
	case TypeTuple:
		tuple := seq.Value.([]*Object)
		if len(tuple) != n {
			return nil, fmt.Errorf("unpack sequence: expected %d items, got %d", n, len(tuple))
		}
		return tuple, nil
	case TypeString:
		s := seq.Value.(string)
		if len(s) != n {
			return nil, fmt.Errorf("unpack sequence: expected %d items, got %d", n, len(s))
		}
		result := make([]*Object, n)
		for i, c := range s {
			result[i] = NewString(string(c))
		}
		return result, nil
	}
	return nil, fmt.Errorf("cannot unpack %s", objectTypeToString(seq.Type))
}

func getIterator(obj *Object) *Object {
	switch obj.Type {
	case TypeList, TypeTuple, TypeString:
		items := obj.Value.([]*Object)
		return &Object{
			Type: TypeList,
			Value: map[string]interface{}{
				"items":  items,
				"index":  0,
				"is_str": obj.Type == TypeString,
			},
		}
	case TypeRange:
		rangeVal := obj.Value.([]int64)
		return &Object{
			Type: TypeList,
			Value: map[string]interface{}{
				"range": rangeVal,
				"index": 0,
			},
		}
	}
	return NewNone()
}

func iteratorNext(iter *Object) (*Object, error) {
	switch iter.Type {
	case TypeList:
		data := iter.Value.(map[string]interface{})
		items := data["items"].([]*Object)
		index := data["index"].(int)

		if index >= len(items) {
			return nil, nil
		}

		result := items[index]
		data["index"] = index + 1
		return result, nil
	}
	return nil, nil
}

var builtins = map[string]*Object{}

func init() {
	builtins["print"] = NewBuiltin(printBuiltin)
	builtins["len"] = NewBuiltin(lenBuiltin)
	builtins["range"] = NewBuiltin(rangeBuiltin)
	builtins["type"] = NewBuiltin(typeBuiltin)
	builtins["str"] = NewBuiltin(strBuiltin)
	builtins["int"] = NewBuiltin(intBuiltin)
	builtins["float"] = NewBuiltin(floatBuiltin)
	builtins["bool"] = NewBuiltin(boolBuiltin)
	builtins["list"] = NewBuiltin(listBuiltin)
	builtins["dict"] = NewBuiltin(dictBuiltin)
	builtins["input"] = NewBuiltin(inputBuiltin)
	builtins["ord"] = NewBuiltin(ordBuiltin)
	builtins["chr"] = NewBuiltin(chrBuiltin)
	builtins["abs"] = NewBuiltin(absBuiltin)
	builtins["min"] = NewBuiltin(minBuiltin)
	builtins["max"] = NewBuiltin(maxBuiltin)
	builtins["sum"] = NewBuiltin(sumBuiltin)
	builtins["reversed"] = NewBuiltin(reversedBuiltin)
	builtins["enumerate"] = NewBuiltin(enumerateBuiltin)
	builtins["zip"] = NewBuiltin(zipBuiltin)
	builtins["map"] = NewBuiltin(mapBuiltin)
	builtins["filter"] = NewBuiltin(filterBuiltin)
	builtins["sorted"] = NewBuiltin(sortedBuiltin)
	builtins["any"] = NewBuiltin(anyBuiltin)
	builtins["all"] = NewBuiltin(allBuiltin)
}

func registerBuiltins(scope *Scope) {
	for k, v := range builtins {
		scope.Builtins[k] = v
	}
}

func printBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) == 0 {
		fmt.Fprintln(vm.Stdout)
		return NewNone(), nil
	}

	parts := make([]string, len(args))
	for i, arg := range args {
		parts[i] = formatObject(arg)
	}
	fmt.Fprintln(vm.Stdout, strings.Join(parts, " "))
	return NewNone(), nil
}

func lenBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("len() takes exactly one argument")
	}
	arg := args[0]
	switch arg.Type {
	case TypeString:
		return NewInt(int64(len(arg.Value.(string)))), nil
	case TypeList:
		return NewInt(int64(len(arg.Value.([]*Object)))), nil
	case TypeTuple:
		return NewInt(int64(len(arg.Value.([]*Object)))), nil
	case TypeDict:
		return NewInt(int64(len(arg.Value.(map[*Object]*Object)))), nil
	}
	return NewInt(0), nil
}

func rangeBuiltin(vm *VM, args []*Object) (*Object, error) {
	var start, stop, step int64

	switch len(args) {
	case 1:
		stop = toInt(args[0])
		start = 0
		step = 1
	case 2:
		start = toInt(args[0])
		stop = toInt(args[1])
		step = 1
	case 3:
		start = toInt(args[0])
		stop = toInt(args[1])
		step = toInt(args[2])
		if step == 0 {
			return nil, fmt.Errorf("range() arg 3 must not be zero")
		}
	default:
		return nil, fmt.Errorf("range() takes 1 to 3 arguments")
	}

	return NewRange(start, stop, step), nil
}

func typeBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("type() takes exactly one argument")
	}
	arg := args[0]
	var typeName string
	switch arg.Type {
	case TypeNone:
		typeName = "NoneType"
	case TypeBool:
		typeName = "bool"
	case TypeInt:
		typeName = "int"
	case TypeFloat:
		typeName = "float"
	case TypeString:
		typeName = "str"
	case TypeList:
		typeName = "list"
	case TypeDict:
		typeName = "dict"
	case TypeTuple:
		typeName = "tuple"
	case TypeFunction:
		typeName = "function"
	case TypeBuiltin:
		typeName = "builtin_function_or_method"
	default:
		typeName = "unknown"
	}
	return NewString(typeName), nil
}

func strBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("str() takes exactly one argument")
	}
	return NewString(formatObject(args[0])), nil
}

func intBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) < 1 || len(args) > 2 {
		return nil, fmt.Errorf("int() takes 1 or 2 arguments")
	}
	arg := args[0]
	base := 10
	if len(args) == 2 {
		base = int(toInt(args[1]))
	}

	switch arg.Type {
	case TypeInt:
		return arg, nil
	case TypeFloat:
		return NewInt(int64(arg.Value.(float64))), nil
	case TypeString:
		s := arg.Value.(string)
		if v, err := strconv.ParseInt(strings.TrimSpace(s), base, 64); err == nil {
			return NewInt(v), nil
		}
		return nil, fmt.Errorf("invalid literal for int(): %q", s)
	case TypeBool:
		if arg.Value.(bool) {
			return NewInt(1), nil
		}
		return NewInt(0), nil
	}
	return NewInt(0), nil
}

func floatBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("float() takes exactly one argument")
	}
	arg := args[0]
	switch arg.Type {
	case TypeFloat:
		return arg, nil
	case TypeInt:
		return NewFloat(float64(arg.Value.(int64))), nil
	case TypeString:
		s := arg.Value.(string)
		if v, err := strconv.ParseFloat(strings.TrimSpace(s), 64); err == nil {
			return NewFloat(v), nil
		}
		return nil, fmt.Errorf("could not convert string to float: %q", s)
	}
	return NewFloat(0), nil
}

func boolBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("bool() takes exactly one argument")
	}
	return NewBool(isTruthy(args[0])), nil
}

func listBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("list() takes exactly one argument")
	}
	arg := args[0]
	switch arg.Type {
	case TypeList:
		items := make([]*Object, len(arg.Value.([]*Object)))
		copy(items, arg.Value.([]*Object))
		return &Object{Type: TypeList, Value: items}, nil
	case TypeTuple:
		items := make([]*Object, len(arg.Value.([]*Object)))
		copy(items, arg.Value.([]*Object))
		return &Object{Type: TypeList, Value: items}, nil
	case TypeString:
		runes := []rune(arg.Value.(string))
		items := make([]*Object, len(runes))
		for i, r := range runes {
			items[i] = NewString(string(r))
		}
		return &Object{Type: TypeList, Value: items}, nil
	}
	return NewList(), nil
}

func dictBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 0 {
		return nil, fmt.Errorf("dict() takes no arguments")
	}
	return NewDict(), nil
}

func inputBuiltin(vm *VM, args []*Object) (*Object, error) {
	var prompt string
	if len(args) > 0 {
		prompt = formatObject(args[0])
	}
	fmt.Print(prompt)
	var line string
	fmt.Scanln(&line)
	return NewString(line), nil
}

func ordBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("ord() takes exactly one argument")
	}
	arg := args[0]
	if arg.Type != TypeString {
		return nil, fmt.Errorf("ord() expected string of length 1")
	}
	s := arg.Value.(string)
	if len(s) != 1 {
		return nil, fmt.Errorf("ord() expected string of length 1")
	}
	return NewInt(int64(s[0])), nil
}

func chrBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("chr() takes exactly one argument")
	}
	arg := args[0]
	c := toInt(arg)
	return NewString(string(rune(c))), nil
}

func absBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("abs() takes exactly one argument")
	}
	arg := args[0]
	if arg.Type == TypeInt {
		v := arg.Value.(int64)
		if v < 0 {
			return NewInt(-v), nil
		}
		return arg, nil
	}
	if arg.Type == TypeFloat {
		v := arg.Value.(float64)
		if v < 0 {
			return NewFloat(-v), nil
		}
		return arg, nil
	}
	return arg, nil
}

func minBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) == 0 {
		return nil, fmt.Errorf("min() expected at least one argument")
	}
	if len(args) == 1 && args[0].Type == TypeList {
		items := args[0].Value.([]*Object)
		if len(items) == 0 {
			return nil, fmt.Errorf("min() arg is an empty sequence")
		}
		minVal := items[0]
		for _, v := range items[1:] {
			if Compare(CompareLess, v, minVal).Value.(bool) {
				minVal = v
			}
		}
		return minVal, nil
	}
	minVal := args[0]
	for _, v := range args[1:] {
		if Compare(CompareLess, v, minVal).Value.(bool) {
			minVal = v
		}
	}
	return minVal, nil
}

func maxBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) == 0 {
		return nil, fmt.Errorf("max() expected at least one argument")
	}
	if len(args) == 1 && args[0].Type == TypeList {
		items := args[0].Value.([]*Object)
		if len(items) == 0 {
			return nil, fmt.Errorf("max() arg is an empty sequence")
		}
		maxVal := items[0]
		for _, v := range items[1:] {
			if Compare(CompareGreater, v, maxVal).Value.(bool) {
				maxVal = v
			}
		}
		return maxVal, nil
	}
	maxVal := args[0]
	for _, v := range args[1:] {
		if Compare(CompareGreater, v, maxVal).Value.(bool) {
			maxVal = v
		}
	}
	return maxVal, nil
}

func sumBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) == 0 {
		return nil, fmt.Errorf("sum() expected at least one argument")
	}
	items := args[0]
	start := NewInt(0)
	if len(args) > 1 {
		start = args[1]
	}

	switch items.Type {
	case TypeList:
		list := items.Value.([]*Object)
		result := start
		for _, v := range list {
			result = binaryOp(result, v, "+")
		}
		return result, nil
	}
	return start, nil
}

func reversedBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("reversed() takes exactly one argument")
	}
	arg := args[0]
	if arg.Type == TypeList {
		list := arg.Value.([]*Object)
		result := make([]*Object, len(list))
		for i, v := range list {
			result[len(list)-1-i] = v
		}
		return &Object{Type: TypeList, Value: result}, nil
	}
	return NewList(), nil
}

func enumerateBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("enumerate() takes exactly one argument")
	}
	arg := args[0]
	if arg.Type == TypeList {
		list := arg.Value.([]*Object)
		result := make([]*Object, len(list))
		for i, v := range list {
			tuple := &Object{Type: TypeTuple, Value: []*Object{NewInt(int64(i)), v}}
			result[i] = tuple
		}
		return &Object{Type: TypeList, Value: result}, nil
	}
	return NewList(), nil
}

func zipBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) < 2 {
		return nil, fmt.Errorf("zip() takes at least two arguments")
	}

	minLen := -1
	for _, arg := range args {
		if arg.Type == TypeList {
			l := len(arg.Value.([]*Object))
			if minLen == -1 || l < minLen {
				minLen = l
			}
		}
	}

	if minLen == 0 {
		return NewList(), nil
	}

	result := make([]*Object, minLen)
	for i := 0; i < minLen; i++ {
		tupleVals := make([]*Object, len(args))
		for j, arg := range args {
			if arg.Type == TypeList {
				list := arg.Value.([]*Object)
				tupleVals[j] = list[i]
			}
		}
		result[i] = &Object{Type: TypeTuple, Value: tupleVals}
	}
	return &Object{Type: TypeList, Value: result}, nil
}

func mapBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 2 {
		return nil, fmt.Errorf("map() takes exactly two arguments")
	}
	fn := args[0]
	items := args[1]

	if fn.Type != TypeBuiltin && fn.Type != TypeFunction {
		return nil, fmt.Errorf("first argument to map() must be a function")
	}

	if items.Type != TypeList {
		return NewList(), nil
	}

	list := items.Value.([]*Object)
	result := make([]*Object, len(list))

	for i, v := range list {
		callArgs := []*Object{v}
		var mapped *Object
		var err error

		if fn.Type == TypeBuiltin {
			fnBuiltin := fn.Value.(BuiltinFunc)
			mapped, err = fnBuiltin(vm, callArgs)
		} else {
			frame := vm.CurrentFrame()
			if frame != nil {
				err = vm.callFunction(frame, fn, callArgs)
				if err == nil {
					mapped = frame.Pop()
				}
			}
		}

		if err != nil {
			return nil, err
		}
		result[i] = mapped
	}

	return &Object{Type: TypeList, Value: result}, nil
}

func filterBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 2 {
		return nil, fmt.Errorf("filter() takes exactly two arguments")
	}
	fn := args[0]
	items := args[1]

	if items.Type != TypeList {
		return NewList(), nil
	}

	list := items.Value.([]*Object)
	result := make([]*Object, 0)

	for _, v := range list {
		callArgs := []*Object{v}
		var keep bool

		if fn.Type == TypeBuiltin {
			fnBuiltin := fn.Value.(BuiltinFunc)
			ret, err := fnBuiltin(vm, callArgs)
			if err != nil {
				return nil, err
			}
			keep = isTruthy(ret)
		}

		if keep {
			result = append(result, v)
		}
	}

	return &Object{Type: TypeList, Value: result}, nil
}

func sortedBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) < 1 {
		return nil, fmt.Errorf("sorted() takes at least one argument")
	}
	items := args[0]
	reverse := false
	if len(args) > 1 {
		reverse = isTruthy(args[1])
	}

	if items.Type != TypeList {
		return NewList(), nil
	}

	list := make([]*Object, len(items.Value.([]*Object)))
	copy(list, items.Value.([]*Object))

	for i := 0; i < len(list); i++ {
		for j := i + 1; j < len(list); j++ {
			cmp := Compare(CompareLess, list[i], list[j]).Value.(bool)
			if reverse {
				cmp = !cmp
			}
			if cmp {
				list[i], list[j] = list[j], list[i]
			}
		}
	}

	return &Object{Type: TypeList, Value: list}, nil
}

func anyBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("any() takes exactly one argument")
	}
	arg := args[0]
	if arg.Type == TypeList {
		for _, v := range arg.Value.([]*Object) {
			if isTruthy(v) {
				return NewBool(true), nil
			}
		}
	}
	return NewBool(false), nil
}

func allBuiltin(vm *VM, args []*Object) (*Object, error) {
	if len(args) != 1 {
		return nil, fmt.Errorf("all() takes exactly one argument")
	}
	arg := args[0]
	if arg.Type == TypeList {
		for _, v := range arg.Value.([]*Object) {
			if !isTruthy(v) {
				return NewBool(false), nil
			}
		}
	}
	return NewBool(true), nil
}

func formatObject(obj *Object) string {
	switch obj.Type {
	case TypeNone:
		return "None"
	case TypeBool:
		if obj.Value.(bool) {
			return "True"
		}
		return "False"
	case TypeInt:
		return fmt.Sprintf("%d", obj.Value.(int64))
	case TypeFloat:
		return fmt.Sprintf("%g", obj.Value.(float64))
	case TypeString:
		return obj.Value.(string)
	case TypeList:
		items := obj.Value.([]*Object)
		parts := make([]string, len(items))
		for i, v := range items {
			parts[i] = formatObject(v)
		}
		return "[" + strings.Join(parts, ", ") + "]"
	case TypeTuple:
		items := obj.Value.([]*Object)
		parts := make([]string, len(items))
		for i, v := range items {
			parts[i] = formatObject(v)
		}
		return "(" + strings.Join(parts, ", ") + ")"
	case TypeDict:
		m := obj.Value.(map[*Object]*Object)
		parts := make([]string, 0)
		for k, v := range m {
			parts = append(parts, formatObject(k)+": "+formatObject(v))
		}
		return "{" + strings.Join(parts, ", ") + "}"
	}
	return obj.String()
}
