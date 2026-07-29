package main

type ProjectMetadata struct {
	Name       string `toml:"name"       json:"nam"`
	Version    string `toml:"version"    json:"ver"`
	Identifier string `toml:"identifier" json:"ide"`
	Creator    string `toml:"creator"    json:"cre"`
	Copyright  string `toml:"copyright"  json:"cop"`
	Url        string `toml:"url"        json:"url"`
	Type       string `toml:"type"       json:"typ"`
}

type ProjectWindowConfig struct {
	Title      string `toml:"title"      json:"tit"`
	Size       []int  `toml:"size"       json:"siz"`
	Fullscreen bool   `toml:"fullscreen" json:"ful"`
	Icon       string `toml:"icon"       json:"ico"`
}

type ProjectRunConfig struct {
	Scene string `toml:"scene" json:"sce"`
}

type ProjectAssets map[string][]string

type ProjectInputConfig map[string]struct {
	Keycode   any       `toml:"keycode"    json:"key,omitempty"`
	Mouse     string    `toml:"mouse"      json:"mou,omitempty"`
	Axis      string    `toml:"axis"       json:"axi,omitempty"`
	AxisRange []float32 `toml:"axis_range" json:"ara,omitempty"`
	Gamepad   string    `toml:"gamepad"    json:"gam,omitempty"`
}

type Project struct {
	Metadata ProjectMetadata     `toml:"metadata" json:"met"`
	Window   ProjectWindowConfig `toml:"window"   json:"win"`
	Run      ProjectRunConfig    `toml:"run"      json:"run"`
	Assets   ProjectAssets       `toml:"assets"   json:"-"`
	Input    ProjectInputConfig  `toml:"input"    json:"inp"`
}
