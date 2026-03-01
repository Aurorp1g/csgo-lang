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
