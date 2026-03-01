// Package vm 提供 CSGO 编程语言的虚拟机实现
//
// 本包实现了 CSGO 语言的字节码解释器
package vm

import (
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"os"
	"strings"
)

const (
	MagicNumber = "CGO\x01"
)

// ObjectType 表示 CSGO 语言的对象类型
type ObjectType int

// 对象类型常量定义
const (
	TypeNone     ObjectType = iota // 空值类型
	TypeBool                       // = 1
	TypeInt                        // = 2
	TypeFloat                      // = 3
	TypeString                     // = 4
	TypeList                       // = 5
	TypeDict                       // = 6
	TypeFunction                   // = 7
	TypeBuiltin                    // = 8
	TypeTuple                      // = 9
	TypeRange                      // = 10
	TypeCell                       // = 11
)

// Object 表示 CSGO 语言的运行时对象
//
// 每个对象包含：
//   - Type: 对象类型
//   - Value: 对象值（实际数据）
//   - RefCount: 引用计数（用于 GC）
//   - Marked: GC 标记位
type Object struct {
	Type     ObjectType
	Value    interface{}
	RefCount int
	Marked   bool
}

func (o *Object) String() string {
	switch o.Type {
	case TypeNone:
		return "None"
	case TypeBool:
		if b, ok := o.Value.(bool); ok {
			return fmt.Sprintf("%v", b)
		}
		return "False"
	case TypeInt:
		return fmt.Sprintf("%v", o.Value)
	case TypeFloat:
		return fmt.Sprintf("%v", o.Value)
	case TypeString:
		return fmt.Sprintf("%q", o.Value)
	case TypeList:
		return "[...]"
	case TypeDict:
		return "{...}"
	case TypeFunction:
		return "<function>"
	case TypeBuiltin:
		return "<builtin>"
	case TypeTuple:
		return "(...)"
	case TypeRange:
		return "range(...)"
	case TypeCell:
		return "<cell>"
	default:
		return "<unknown>"
	}
}

func NewNone() *Object {
	return &Object{Type: TypeNone, Value: nil}
}

func NewBool(b bool) *Object {
	return &Object{Type: TypeBool, Value: b}
}

func NewInt(i int64) *Object {
	return &Object{Type: TypeInt, Value: i}
}

func NewFloat(f float64) *Object {
	return &Object{Type: TypeFloat, Value: f}
}

func NewString(s string) *Object {
	return &Object{Type: TypeString, Value: s}
}

func NewList() *Object {
	return &Object{Type: TypeList, Value: []*Object{}}
}

func NewDict() *Object {
	return &Object{Type: TypeDict, Value: map[*Object]*Object{}}
}

func NewTuple(n int) *Object {
	return &Object{Type: TypeTuple, Value: make([]*Object, n)}
}

func NewRange(start, stop, step int64) *Object {
	return &Object{Type: TypeRange, Value: []int64{start, stop, step}}
}

type BuiltinFunc func(*VM, []*Object) (*Object, error)

func NewBuiltin(f BuiltinFunc) *Object {
	return &Object{Type: TypeBuiltin, Value: f}
}

type Function struct {
	Name     string
	Code     *CodeObject
	Defaults []*Object
	Closure  []*Object
}

func NewFunction(name string, code *CodeObject) *Object {
	return &Object{
		Type:  TypeFunction,
		Value: &Function{Name: name, Code: code},
	}
}

type CodeObject struct {
	Name              string
	Filename          string
	ArgCount          uint16
	PosOnlyArgCount   uint16
	KwOnlyArgCount    uint16
	NLocals           uint16
	StackSize         uint16
	FirstLineNo       uint16
	Flags             uint32
	Instructions      []byte
	Constants         []*Object
	Names             []string
	VarNames          []string
	FreeVars          []string
	CellVars          []string
	ExceptionHandlers []ExceptionHandler
}

type ExceptionHandler struct {
	Start      uint16
	End        uint16
	Target     uint16
	Type       uint8
	StackDepth uint16
}

func (c *CodeObject) String() string {
	return fmt.Sprintf("<code %q>", c.Name)
}

func ReadCodeObject(r io.Reader) (*CodeObject, error) {
	data, err := io.ReadAll(r)
	if err != nil {
		return nil, fmt.Errorf("failed to read file: %w", err)
	}

	code := &CodeObject{}

	if len(data) < 4 {
		return nil, fmt.Errorf("file too small for magic number")
	}

	magic := string(data[0:4])
	if magic != MagicNumber {
		return nil, fmt.Errorf("invalid magic number: %q", magic)
	}

	offset := 4

	nameLen := binary.LittleEndian.Uint32(data[offset : offset+4])
	offset += 4

	if offset+int(nameLen) > len(data) {
		return nil, fmt.Errorf("truncated file at name")
	}
	code.Name = string(data[offset : offset+int(nameLen)])
	offset += int(nameLen)

	if offset+4 > len(data) {
		return nil, fmt.Errorf("truncated file at flags")
	}
	code.Flags = binary.LittleEndian.Uint32(data[offset : offset+4])
	offset += 4

	if offset+2 > len(data) {
		return nil, fmt.Errorf("truncated file at argcount")
	}
	code.ArgCount = binary.LittleEndian.Uint16(data[offset : offset+2])
	offset += 2

	if offset+2 > len(data) {
		return nil, fmt.Errorf("truncated file at posonlyargcount")
	}
	code.PosOnlyArgCount = binary.LittleEndian.Uint16(data[offset : offset+2])
	offset += 2

	if offset+2 > len(data) {
		return nil, fmt.Errorf("truncated file at kwonlyargcount")
	}
	code.KwOnlyArgCount = binary.LittleEndian.Uint16(data[offset : offset+2])
	offset += 2

	if offset+2 > len(data) {
		return nil, fmt.Errorf("truncated file at nlocals")
	}
	code.NLocals = binary.LittleEndian.Uint16(data[offset : offset+2])
	offset += 2

	if offset+2 > len(data) {
		return nil, fmt.Errorf("truncated file at stacksize")
	}
	code.StackSize = binary.LittleEndian.Uint16(data[offset : offset+2])
	offset += 2

	if offset+4 > len(data) {
		return nil, fmt.Errorf("truncated file at instruction_count")
	}
	instrCount := binary.LittleEndian.Uint32(data[offset : offset+4])
	_ = instrCount
	offset += 4

	if offset+2 > len(data) {
		return nil, fmt.Errorf("truncated file at firstlineno")
	}
	code.FirstLineNo = binary.LittleEndian.Uint16(data[offset : offset+2])
	offset += 2

	codeStart := offset

	offsetToInstr := make(map[int]int)
	instrToOffset := make(map[int]int)
	idx := codeStart
	instrIdx := 0

	for idx < len(data) && instrIdx < int(instrCount) {
		offsetToInstr[idx] = instrIdx
		instrToOffset[instrIdx] = idx

		if idx >= len(data) {
			break
		}

		opcode := data[idx]

		if HasArg[Opcode(opcode)] {
			if idx+3 > len(data) {
				break
			}
			idx += 3
		} else {
			idx += 1
		}
		instrIdx++

		if instrIdx >= 1024 {
			break
		}
	}

	codeSize := 0
	if lastOffset, ok := instrToOffset[instrIdx-1]; ok {
		lastOp := data[lastOffset]
		if HasArg[Opcode(lastOp)] {
			codeSize = (lastOffset - codeStart) + 3
		} else {
			codeSize = (lastOffset - codeStart) + 1
		}
	}

	constsOffset := codeStart + codeSize

	if constsOffset+4 > len(data) {
		code.Constants = []*Object{}
	} else {
		constsCount := binary.LittleEndian.Uint32(data[constsOffset : constsOffset+4])
		code.Constants = make([]*Object, constsCount)

		constIdx := constsOffset + 4
		for i := uint32(0); i < constsCount; i++ {
			constant, consumed, err := parseConstant(data, constIdx)
			if err != nil {
				return nil, fmt.Errorf("failed to read constant %d: %w", i, err)
			}
			code.Constants[i] = constant
			constIdx += consumed
		}
	}

	namesOffset := constsOffset + 4
	{
		constIdx := constsOffset + 4
		for i := uint32(0); i < uint32(len(code.Constants)); i++ {
			if constIdx >= len(data) {
				break
			}
			_, consumed, _ := parseConstant(data, constIdx)
			constIdx += consumed
		}
		namesOffset = constIdx
	}

	if namesOffset+4 > len(data) {
		code.Names = []string{}
	} else {
		namesCount := binary.LittleEndian.Uint32(data[namesOffset : namesOffset+4])
		code.Names = make([]string, namesCount)

		nameIdx := namesOffset + 4
		for i := uint32(0); i < namesCount; i++ {
			if nameIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[nameIdx : nameIdx+4])
			nameIdx += 4

			if nameIdx+int(strLen) > len(data) {
				break
			}
			code.Names[i] = string(data[nameIdx : nameIdx+int(strLen)])
			nameIdx += int(strLen)
		}
	}

	varnamesOffset := namesOffset + 4
	{
		nameIdx := namesOffset + 4
		for i := uint32(0); i < uint32(len(code.Names)); i++ {
			if nameIdx >= len(data) {
				break
			}
			if nameIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[nameIdx : nameIdx+4])
			nameIdx += 4 + int(strLen)
		}
		varnamesOffset = nameIdx
	}

	if varnamesOffset+4 > len(data) {
		code.VarNames = []string{}
	} else {
		varnamesCount := binary.LittleEndian.Uint32(data[varnamesOffset : varnamesOffset+4])
		code.VarNames = make([]string, varnamesCount)

		varIdx := varnamesOffset + 4
		for i := uint32(0); i < varnamesCount; i++ {
			if varIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[varIdx : varIdx+4])
			varIdx += 4

			if varIdx+int(strLen) > len(data) {
				break
			}
			code.VarNames[i] = string(data[varIdx : varIdx+int(strLen)])
			varIdx += int(strLen)
		}
	}

	freevarsOffset := varnamesOffset + 4
	{
		varIdx := varnamesOffset + 4
		for i := uint32(0); i < uint32(len(code.VarNames)); i++ {
			if varIdx >= len(data) {
				break
			}
			if varIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[varIdx : varIdx+4])
			varIdx += 4 + int(strLen)
		}
		freevarsOffset = varIdx
	}

	if freevarsOffset+4 > len(data) {
		code.FreeVars = []string{}
	} else {
		freevarsCount := binary.LittleEndian.Uint32(data[freevarsOffset : freevarsOffset+4])
		code.FreeVars = make([]string, freevarsCount)

		freeIdx := freevarsOffset + 4
		for i := uint32(0); i < freevarsCount; i++ {
			if freeIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[freeIdx : freeIdx+4])
			freeIdx += 4

			if freeIdx+int(strLen) > len(data) {
				break
			}
			code.FreeVars[i] = string(data[freeIdx : freeIdx+int(strLen)])
			freeIdx += int(strLen)
		}
	}

	cellvarsOffset := freevarsOffset + 4
	{
		freeIdx := freevarsOffset + 4
		for i := uint32(0); i < uint32(len(code.FreeVars)); i++ {
			if freeIdx >= len(data) {
				break
			}
			if freeIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[freeIdx : freeIdx+4])
			freeIdx += 4 + int(strLen)
		}
		cellvarsOffset = freeIdx
	}

	if cellvarsOffset+4 > len(data) {
		code.CellVars = []string{}
	} else {
		cellvarsCount := binary.LittleEndian.Uint32(data[cellvarsOffset : cellvarsOffset+4])
		code.CellVars = make([]string, cellvarsCount)

		cellIdx := cellvarsOffset + 4
		for i := uint32(0); i < cellvarsCount; i++ {
			if cellIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[cellIdx : cellIdx+4])
			cellIdx += 4

			if cellIdx+int(strLen) > len(data) {
				break
			}
			code.CellVars[i] = string(data[cellIdx : cellIdx+int(strLen)])
			cellIdx += int(strLen)
		}
	}

	handlersOffset := cellvarsOffset + 4
	{
		cellIdx := cellvarsOffset + 4
		for i := uint32(0); i < uint32(len(code.CellVars)); i++ {
			if cellIdx >= len(data) {
				break
			}
			if cellIdx+4 > len(data) {
				break
			}
			strLen := binary.LittleEndian.Uint32(data[cellIdx : cellIdx+4])
			cellIdx += 4 + int(strLen)
		}
		handlersOffset = cellIdx
	}

	if handlersOffset+4 > len(data) {
		code.ExceptionHandlers = []ExceptionHandler{}
	} else {
		handlersCount := binary.LittleEndian.Uint32(data[handlersOffset : handlersOffset+4])
		code.ExceptionHandlers = make([]ExceptionHandler, handlersCount)

		hdrIdx := handlersOffset + 4
		for i := uint32(0); i < handlersCount; i++ {
			if hdrIdx+9 > len(data) {
				break
			}
			handler := ExceptionHandler{
				Start:      binary.LittleEndian.Uint16(data[hdrIdx : hdrIdx+2]),
				End:        binary.LittleEndian.Uint16(data[hdrIdx+2 : hdrIdx+4]),
				Target:     binary.LittleEndian.Uint16(data[hdrIdx+4 : hdrIdx+6]),
				Type:       data[hdrIdx+6],
				StackDepth: binary.LittleEndian.Uint16(data[hdrIdx+7 : hdrIdx+9]),
			}
			code.ExceptionHandlers[i] = handler
			hdrIdx += 9
		}
	}

	lnotabOffset := handlersOffset + 4 + 9*len(code.ExceptionHandlers)
	_ = lnotabOffset

	code.Instructions = data[codeStart:constsOffset]

	return code, nil
}

func parseConstant(data []byte, offset int) (*Object, int, error) {
	if offset >= len(data) {
		return nil, 0, fmt.Errorf("EOF while reading constant type")
	}

	constType := data[offset]

	switch constType {
	case 0x00:
		return NewNone(), 1, nil
	case 0x01:
		if offset+2 > len(data) {
			return nil, 0, fmt.Errorf("EOF while reading bool value")
		}
		return NewBool(data[offset+1] != 0), 2, nil
	case 0x02:
		if offset+9 > len(data) {
			return nil, 0, fmt.Errorf("EOF while reading int value")
		}
		val := int64(binary.LittleEndian.Uint64(data[offset+1 : offset+9]))
		return NewInt(val), 9, nil
	case 0x03:
		if offset+9 > len(data) {
			return nil, 0, fmt.Errorf("EOF while reading float value")
		}
		val := binary.LittleEndian.Uint64(data[offset+1 : offset+9])
		return NewFloat(float64(math.Float64frombits(val))), 9, nil
	case 0x04:
		if offset+5 > len(data) {
			return nil, 0, fmt.Errorf("EOF while reading string length")
		}
		strLen := binary.LittleEndian.Uint32(data[offset+1 : offset+5])
		if offset+5+int(strLen) > len(data) {
			return nil, 0, fmt.Errorf("EOF while reading string value")
		}
		return NewString(string(data[offset+5 : offset+5+int(strLen)])), 5 + int(strLen), nil
	default:
		return NewNone(), 1, nil
	}
}

func LoadCodeObject(filename string) (*CodeObject, error) {
	f, err := os.Open(filename)
	if err != nil {
		return nil, fmt.Errorf("failed to open file %s: %w", filename, err)
	}
	defer f.Close()

	code, err := ReadCodeObject(f)
	if err != nil {
		return nil, fmt.Errorf("failed to read code object from %s: %w", filename, err)
	}

	code.Filename = filename

	return code, nil
}

type CompareOp int

const (
	CompareLess CompareOp = iota
	CompareLessEqual
	CompareEqual
	CompareNotEqual
	CompareGreater
	CompareGreaterEqual
	CompareIn
	CompareNotIn
	CompareIs
	CompareIsNot
	CompareExceptionMatch
)

func Compare(op CompareOp, a, b *Object) *Object {
	switch op {
	case CompareLess:
		if a.Type == TypeInt && b.Type == TypeInt {
			return NewBool(a.Value.(int64) < b.Value.(int64))
		}
		return NewBool(false)
	case CompareLessEqual:
		if a.Type == TypeInt && b.Type == TypeInt {
			return NewBool(a.Value.(int64) <= b.Value.(int64))
		}
		return NewBool(false)
	case CompareEqual:
		return NewBool(equal(a, b))
	case CompareNotEqual:
		return NewBool(!equal(a, b))
	case CompareGreater:
		if a.Type == TypeInt && b.Type == TypeInt {
			return NewBool(a.Value.(int64) > b.Value.(int64))
		}
		return NewBool(false)
	case CompareGreaterEqual:
		if a.Type == TypeInt && b.Type == TypeInt {
			return NewBool(a.Value.(int64) >= b.Value.(int64))
		}
		return NewBool(false)
	case CompareIs, CompareIsNot:
		same := same(a, b)
		if op == CompareIs {
			return NewBool(same)
		}
		return NewBool(!same)
	case CompareIn, CompareNotIn:
		found := contains(a, b)
		if op == CompareIn {
			return NewBool(found)
		}
		return NewBool(!found)
	}
	return NewNone()
}

func equal(a, b *Object) bool {
	if a.Type != b.Type {
		if a.Type == TypeInt && b.Type == TypeFloat {
			return float64(a.Value.(int64)) == b.Value.(float64)
		}
		if a.Type == TypeFloat && b.Type == TypeInt {
			return a.Value.(float64) == float64(b.Value.(int64))
		}
		return false
	}
	switch a.Type {
	case TypeNone:
		return true
	case TypeBool:
		return a.Value.(bool) == b.Value.(bool)
	case TypeInt:
		return a.Value.(int64) == b.Value.(int64)
	case TypeFloat:
		return a.Value.(float64) == b.Value.(float64)
	case TypeString:
		return a.Value.(string) == b.Value.(string)
	}
	return false
}

func same(a, b *Object) bool {
	return a == b
}

func contains(container, item *Object) bool {
	switch container.Type {
	case TypeList:
		for _, v := range container.Value.([]*Object) {
			if equal(v, item) {
				return true
			}
		}
	case TypeString:
		if item.Type == TypeString {
			return strings.Contains(container.Value.(string), item.Value.(string))
		}
	case TypeDict:
		_, ok := container.Value.(map[*Object]*Object)[item]
		return ok
	}
	return false
}
