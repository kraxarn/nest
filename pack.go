package main

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"unsafe"

	"github.com/BurntSushi/toml"
)

type Asset struct {
	path string
	name string
}

func writeHeader(file *os.File, fileCount uint32) (uint32, error) {
	err := binary.Write(file, binary.LittleEndian, nestMagic)
	if err != nil {
		return 0, err
	}

	err = binary.Write(file, binary.LittleEndian, nestVersion)
	if err != nil {
		return 0, err
	}

	err = binary.Write(file, binary.LittleEndian, fileCount)
	if err != nil {
		return 0, err
	}

	written := unsafe.Sizeof(nestMagic) + unsafe.Sizeof(nestVersion) + unsafe.Sizeof(fileCount)
	return uint32(written), nil
}

func writeFileDescriptor(file *os.File, offset *uint32, name string, size uint32) error {
	hash := murmur3([]byte(name), uint32(len(name)))
	err := binary.Write(file, binary.LittleEndian, hash)
	if err != nil {
		return err
	}

	flags := uint16(0)
	err = binary.Write(file, binary.LittleEndian, flags)
	if err != nil {
		return err
	}

	err = binary.Write(file, binary.LittleEndian, *offset)
	if err != nil {
		return err
	}

	err = binary.Write(file, binary.LittleEndian, size)
	if err != nil {
		return err
	}

	*offset += size
	return nil
}

func writeAssetDescriptor(file *os.File, offset *uint32, asset Asset) error {
	hash := murmur3([]byte(asset.name), uint32(len(asset.name)))
	err := binary.Write(file, binary.LittleEndian, hash)
	if err != nil {
		return err
	}

	flags := uint16(0)
	err = binary.Write(file, binary.LittleEndian, flags)
	if err != nil {
		return err
	}

	stat, err := os.Stat(asset.path)
	if err != nil {
		return err
	}

	err = binary.Write(file, binary.LittleEndian, *offset)
	if err != nil {
		return err
	}

	err = binary.Write(file, binary.LittleEndian, uint32(stat.Size()))
	if err != nil {
		return err
	}

	*offset += uint32(stat.Size())
	return nil
}

func parseProject(path string) (Project, error) {
	var project Project
	_, err := toml.DecodeFile(path, &project)
	if err != nil {
		return Project{}, err
	}

	for key, input := range project.Input {
		if keycode, ok := input.Keycode.(string); ok {
			input.Keycode = []string{keycode}
			project.Input[key] = input
		}
	}

	return project, nil
}

func supportedFile(path string) bool {
	ext := filepath.Ext(path)
	allExt := []string{".qoi", ".gltf", ".py"}
	return slices.Contains(allExt, ext)
}

func findFile(path string) (string, error) {
	matches, err := filepath.Glob(fmt.Sprintf("%s.*", path))
	if err != nil {
		return "", err
	}

	if matches == nil {
		return "", errors.New("no file found")
	}

	var results []string
	for _, match := range matches {
		if supportedFile(match) {
			results = append(results, match)
		}
	}

	if results == nil {
		return "", errors.New("no supported file found")
	}

	if len(results) > 1 {
		return "", errors.New("more than one file found")
	}

	return results[0], nil
}

func collectAssets(projectAssets ProjectAssets, dir string) ([]Asset, error) {
	var assets []Asset

	// To make sure we iterate in the same order every time
	var keys []string
	for key := range projectAssets {
		keys = append(keys, key)
	}
	slices.Sort(keys)

	for _, key := range keys {
		for _, value := range projectAssets[key] {
			path, err := findFile(fmt.Sprintf("%s/%s/%s", dir, key, value))
			if err != nil {
				return nil, fmt.Errorf("failed to pack %s/%s: %v", key, value, err)
			}
			assets = append(assets, Asset{
				path: path,
				name: fmt.Sprintf("%s/%s", key, value),
			})

			if filepath.Ext(path) == ".gltf" {
				var gltfData []byte
				gltfData, err = os.ReadFile(path)
				if err != nil {
					return nil, err
				}

				var asset GltfAsset
				if err = json.Unmarshal(gltfData, &asset); err != nil {
					return nil, err
				}

				for _, buffer := range asset.Buffers {
					assets = append(assets, Asset{
						path: fmt.Sprintf("%s/%s", filepath.Dir(path), buffer.Uri),
						name: fmt.Sprintf("models/buffers/%s",
							strings.TrimSuffix(buffer.Uri, filepath.Ext(buffer.Uri)),
						),
					})
				}

				// We might want to do extra stuff with images later
				for _, buffer := range asset.Images {
					assets = append(assets, Asset{
						path: fmt.Sprintf("%s/%s", filepath.Dir(path), buffer.Uri),
						name: fmt.Sprintf("models/images/%s",
							strings.TrimSuffix(buffer.Uri, filepath.Ext(buffer.Uri)),
						),
					})
				}
			}
		}
	}

	return assets, nil
}

func pack(inPath, outPath string) error {
	file, err := os.OpenFile(outPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		return err
	}

	defer func(file *os.File) {
		err = file.Close()
		if err != nil {
			panic(err)
		}
	}(file)

	var project Project
	project, err = parseProject(inPath)

	var assets []Asset
	inDir := filepath.Dir(inPath)
	assets, err = collectAssets(project.Assets, inDir)
	if err != nil {
		return err
	}

	fmt.Printf("packing %d files\n", len(assets))

	var offset uint32
	offset, err = writeHeader(file, uint32(len(assets)+1))
	if err != nil {
		return err
	}

	descSize := (unsafe.Sizeof(uint32(0)) * 3) + unsafe.Sizeof(uint16(0))
	offset += uint32(descSize) * uint32(len(assets)+1)

	var jsonData []byte
	jsonData, err = json.Marshal(project)
	if err != nil {
		return err
	}

	err = writeFileDescriptor(file, &offset, "project", uint32(len(jsonData)))
	if err != nil {
		return err
	}

	for _, asset := range assets {
		fmt.Printf("packing: %s -> %s\n", asset.path[len(inDir):], asset.name)
		if err = writeAssetDescriptor(file, &offset, asset); err != nil {
			return err
		}
	}

	_, err = file.Write(jsonData)
	if err != nil {
		return err
	}

	for _, asset := range assets {
		var content []byte
		content, err = os.ReadFile(asset.path)
		if err != nil {
			return err
		}
		if _, err = file.Write(content); err != nil {
			return err
		}
	}

	return nil
}
