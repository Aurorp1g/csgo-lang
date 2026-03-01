// Package main 是 CSGO 编程语言的虚拟机程序入口
//
// 本程序是 CSGO 语言的运行时解释器（cgr - CSGO Runner）。
// 加载并执行 .cgb 字节码文件。
//
// 使用方法：
//
//	cgr <file.cgb>
//
// 编译流程：
//  1. 使用 cgc 编译器将 .cg 源文件编译为 .cgb 字节码
//  2. 使用 cgr 运行器执行 .cgb 字节码文件
//
// 参考资料：
//   - vm 包：虚拟机实现
package main

import (
	"fmt"
	"os"

	"csgo-language/go/runtime/vm"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: csgo <file.cgb>")
		os.Exit(1)
	}

	filename := os.Args[1]
	code, err := vm.LoadCodeObject(filename)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error loading bytecode: %v\n", err)
		os.Exit(1)
	}

	v := vm.NewVM()
	err = v.Run(code)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}
