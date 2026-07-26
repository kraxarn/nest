package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"path/filepath"
)

func unpack(inPath, outPath string) error {
	file, err := os.OpenFile(inPath, os.O_RDONLY, 0644)
	if err != nil {
		return err
	}

	defer func(file *os.File) {
		err = file.Close()
		if err != nil {
			panic(err)
		}
	}(file)

	// Error hopefully means the directory already exists
	_ = os.Mkdir(outPath, 0755)

	var magic uint32
	if err = binary.Read(file, binary.LittleEndian, &magic); err != nil {
		return err
	}
	if magic != nestMagic {
		return errors.New("file is not a valid nest file")
	}

	var version uint8
	if err = binary.Read(file, binary.LittleEndian, &version); err != nil {
		return err
	}
	if version != nestVersion {
		return fmt.Errorf("unknown nest version: %d", version)
	}

	var fileCount uint32
	if err = binary.Read(file, binary.LittleEndian, &fileCount); err != nil {
		return err
	}

	fmt.Printf("unpacking %d files\n", fileCount)

	for i := 0; i < int(fileCount); i++ {
		var hash uint32
		if err = binary.Read(file, binary.LittleEndian, &hash); err != nil {
			return err
		}

		path := fmt.Sprintf("%s/%08x.bin", outPath, hash)
		fmt.Printf("unpacking: %s\n", filepath.Base(path))

		var flags uint16
		if err = binary.Read(file, binary.LittleEndian, &flags); err != nil {
			return err
		}

		if flags != 0 {
			return fmt.Errorf("unknown flags: %d", flags)
		}

		var offset uint32
		if err = binary.Read(file, binary.LittleEndian, &offset); err != nil {
			return err
		}

		var size uint32
		if err = binary.Read(file, binary.LittleEndian, &size); err != nil {
			return err
		}

		data := make([]byte, size)
		if _, err = file.ReadAt(data, int64(offset)); err != nil {
			return err
		}

		if err = os.WriteFile(path, data, 0644); err != nil {
			return err
		}
	}

	return nil
}
