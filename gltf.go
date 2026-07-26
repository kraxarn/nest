package main

type GltfBuffer struct {
	Uri string `json:"uri"`
}

type GltfAsset struct {
	Images  []GltfBuffer `json:"images"`
	Buffers []GltfBuffer `json:"buffers"`
}
