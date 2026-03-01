// Package main 是 CSGO 编程语言的主入口程序
//
// 本程序是 CSGO 语言的统一入口（csgo），支持直接运行 .cg 源文件。
// 自动调用编译器和虚拟机，无需手动分步执行。
//
// 目录结构：
//   - dist/cg_compiler/cgc.exe: 编译器
//   - dist/cg_vm/cgr.exe: 虚拟机
//
// 参考资料：
//   - cgc: 编译器
//   - cgr: 虚拟机
package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

const (
	distDir          = "dist"        // 发布目录
	cgcDir           = "cg_compiler" // 编译器目录
	cgrDir           = "cg_vm"       // 虚拟机目录
	compilerFileName = "cgc.exe"     // 编译器文件名
	vmFileName       = "cgr.exe"     // 虚拟机文件名
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: csgo <source.cg> [args...]")
		os.Exit(1)
	}

	execDir := getExecDir()
	compilerPath := filepath.Join(execDir, cgcDir, compilerFileName)
	vmPath := filepath.Join(execDir, cgrDir, vmFileName)

	if _, err := os.Stat(compilerPath); os.IsNotExist(err) {
		fmt.Fprintf(os.Stderr, "Error: compiler not found: %s\n", compilerPath)
		os.Exit(1)
	}

	sourceFile := os.Args[1]
	if !strings.HasSuffix(sourceFile, ".cg") {
		forwardArgs := os.Args[1:]
		cmd := exec.Command(compilerPath, forwardArgs...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		cmd.Stdin = os.Stdin
		cmd.Dir = execDir

		if err := cmd.Run(); err != nil {
			fmt.Fprintf(os.Stderr, "Error: %v\n", err)
			os.Exit(1)
		}
		return
	}

	if _, err := os.Stat(sourceFile); os.IsNotExist(err) {
		fmt.Fprintf(os.Stderr, "Error: source file not found: %s\n", sourceFile)
		os.Exit(1)
	}

	if _, err := os.Stat(vmPath); os.IsNotExist(err) {
		fmt.Fprintf(os.Stderr, "Error: VM not found: %s\n", vmPath)
		os.Exit(1)
	}

	baseName := strings.TrimSuffix(sourceFile, ".cg")
	bytecodeFile := baseName + ".cgb"

	cgcArgs := append([]string{sourceFile, bytecodeFile}, os.Args[2:]...)
	compileCmd := exec.Command(compilerPath, cgcArgs...)
	compileCmd.Stdout = os.Stdout
	compileCmd.Stderr = os.Stderr
	compileCmd.Stdin = os.Stdin

	if err := compileCmd.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Compilation failed: %v\n", err)
		os.Exit(1)
	}

	if _, err := os.Stat(bytecodeFile); os.IsNotExist(err) {
		fmt.Fprintf(os.Stderr, "Error: bytecode file was not generated\n")
		os.Exit(1)
	}

	fmt.Printf("\n")

	vmArgs := os.Args[2:]
	vmCmd := exec.Command(vmPath, append([]string{bytecodeFile}, vmArgs...)...)
	vmCmd.Stdout = os.Stdout
	vmCmd.Stderr = os.Stderr
	vmCmd.Stdin = os.Stdin

	if err := vmCmd.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Execution failed: %v\n", err)
		os.Exit(1)
	}
}

func getExecDir() string {
	execPath, err := os.Executable()
	if err != nil {
		return "."
	}
	return filepath.Dir(execPath)
}
