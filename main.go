package main

import (
	"flag"
	"path/filepath"
)

const nestMagic = uint32('n')<<0 | uint32('e')<<8 | uint32('s')<<16 | uint32('t')<<24

const nestVersion uint8 = 1

type Flags struct {
	inPath  string
	outPath string
}

func parseFlags() Flags {
	flags := Flags{}

	flag.StringVar(&flags.inPath, "in", "", "path to input file/folder")
	flag.StringVar(&flags.outPath, "out", "", "path to output file/folder")

	flag.Parse()

	return flags
}

func main() {
	flags := parseFlags()

	inExt := filepath.Ext(flags.inPath)
	outExt := filepath.Ext(flags.outPath)

	if inExt == ".toml" && outExt == ".nest" {
		if err := pack(flags.inPath, flags.outPath); err != nil {
			panic(err)
		}
	} else if inExt == ".nest" && outExt == "" {
		if err := unpack(flags.inPath, flags.outPath); err != nil {
			panic(err)
		}
	} else {
		panic("i dunno what you want me to do")
	}
}
