#include "tomlc17.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <jansson.h>

uint32_t murmur3_32(const void *data, size_t len, uint32_t seed);

static constexpr uint32_t nest_magic = ((uint8_t) 'n') << 0
	| ((uint8_t) 'e') << 8
	| ((uint8_t) 's') << 16
	| ((uint8_t) 't') << 24;

[[nodiscard]]
static bool supported_file(const char *filename)
{
	// I'm not sure if I like this, but it looks cool
#define str_len(s) ((sizeof(s) / sizeof((s)[0])) - sizeof((s)[0]))
#define has_ext(e) (len >= str_len(e) && strcmp(filename + len - str_len(e), e) == 0)

	const size_t len = strlen(filename);

	return has_ext(".qoi")
		|| has_ext(".glb");

#undef has_ext
#undef str_len
}

[[nodiscard]]
static char *find_file(const char *parent, const char *filename)
{
	DIR *dir = opendir(parent);
	if (dir == nullptr)
	{
		return nullptr;
	}

	const struct dirent *entry = nullptr;
	while ((entry = readdir(dir)) != nullptr)
	{
		if (!supported_file(entry->d_name))
		{
			continue;
		}

		char *name = strdup(entry->d_name);
		strrchr(name, '.')[0] = '\0';

		if (strcmp(name, filename) != 0)
		{
			free(name);
			continue;
		}

		char *full_path = nullptr;
		asprintf(&full_path, "%s/%s", parent, entry->d_name);

		free(name);
		closedir(dir);
		return full_path;
	}

	closedir(dir);
	return nullptr;
}

[[nodiscard]]
static size_t file_size(FILE *file)
{
	size_t size = 0;
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	return size;
}

static void copy(FILE *src, FILE *dst)
{
	constexpr size_t buffer_size = 8192;
	uint8_t buffer[buffer_size];

	size_t read = 0;
	while ((read = fread(buffer, sizeof(uint8_t), buffer_size, src)) > 0)
	{
		fwrite(buffer, sizeof(uint8_t), read, dst);
	}
}

static void write_desc(FILE *file, const size_t size, const char *str,
	const size_t str_len, uint32_t *offset)
{
	const uint32_t hash = murmur3_32(str, str_len, str_len);
	fwrite(&hash, sizeof(uint32_t), 1, file);

	constexpr uint16_t flags = 0;
	fwrite(&flags, sizeof(uint16_t), 1, file);

	fwrite(offset, sizeof(uint32_t), 1, file);
	*offset += size;

	fwrite(&size, sizeof(uint32_t), 1, file);
}

static char *project(const toml_datum_t table, size_t *size)
{
	const toml_datum_t *window_size = toml_seek(table, "window.size").u.arr.elem;

	json_t *json = json_pack("{s{ss, ss, ss, ss, ss, ss, ss}, s{ss, s[i, i], sb, ss}}",
		"met",
		"nam", toml_seek(table, "metadata.name").u.s,
		"ver", toml_seek(table, "metadata.version").u.s,
		"ide", toml_seek(table, "metadata.identifier").u.s,
		"cre", toml_seek(table, "metadata.creator").u.s,
		"cop", toml_seek(table, "metadata.copyright").u.s,
		"url", toml_seek(table, "metadata.url").u.s,
		"typ", toml_seek(table, "metadata.type").u.s,
		"win",
		"tit", toml_seek(table, "window.title").u.s,
		"siz", window_size[0].u.int64, window_size[1].u.int64,
		"ful", (int) toml_seek(table, "window.fullscreen").u.boolean,
		"ico", toml_seek(table, "window.icon").u.s,
		"run",
		"sce", toml_seek(table, "run.scene").u.s
	);

	const toml_datum_t input = toml_seek(table, "input");
	json_t *inp = json_object();

	for (int32_t t1 = 0; t1 < input.u.tab.size; t1++)
	{
		const char *key = input.u.tab.key[t1];
		const toml_datum_t value = input.u.tab.value[t1];

		json_t *inp_value = json_pack("{ss*, ss*, ss*}",
			"key", toml_get(value, "keycode").u.s,
			"mou", toml_get(value, "mouse").u.s,
			"axi", toml_get(value, "axis").u.s
		);

		const toml_datum_t axis_range = toml_get(value, "axis_range");
		if (axis_range.type == TOML_ARRAY)
		{
			json_object_set(inp_value, "ara", json_pack("[f, f]",
				axis_range.u.arr.elem[0].u.fp64,
				axis_range.u.arr.elem[1].u.fp64
			));
		}

		json_object_set(inp, key, inp_value);
	}

	json_object_set(json, "inp", inp);

	constexpr size_t flags = JSON_COMPACT;
	*size = json_dumpb(json, nullptr, 0, flags);
	char *buffer = malloc(*size);
	json_dumpb(json, buffer, *size, flags);

	json_decref(json);
	return buffer;
}

static bool pack(const char *path)
{
	char *in_path = nullptr;
	asprintf(&in_path, "%s/%s", path, "project.toml");

	FILE *in_file = fopen(in_path, "r");
	free(in_path);

	if (in_file == nullptr)
	{
		fprintf(stderr, "Failed to open output path: Error %d\n", errno);
		return false;
	}

	char *out_path = nullptr;
	asprintf(&out_path, "%s/%s", path, "assets.nest");

	FILE *out_file = fopen(out_path, "wb");
	free(out_path);

	if (out_file == nullptr)
	{
		fprintf(stderr, "Failed to open output path: Error %d\n", errno);
		fclose(in_file);
		return false;
	}

	const toml_result_t result = toml_parse_file(in_file);
	if (!result.ok)
	{
		fprintf(stderr, "Failed to parse project file: %s\n", result.errmsg);
		fclose(in_file);
		fclose(out_file);
		return false;
	}

	const toml_datum_t assets = toml_get(result.toptab, "assets");
	if (assets.type != TOML_TABLE)
	{
		fprintf(stderr, "Invalid project: 'assets' invalid or not found\n");
		fclose(in_file);
		fclose(out_file);
		return false;
	}

	uint32_t file_count = 1;
	for (int32_t i = 0; i < assets.u.tab.size; i++)
	{
		const toml_datum_t value = assets.u.tab.value[i];
		if (value.type == TOML_ARRAY)
		{
			file_count += value.u.arr.size;
		}
	}

	printf("found %d %s\n", file_count - 1, file_count == 2 ? "file" : "files");

	// Header

	fwrite(&nest_magic, sizeof(uint32_t), 1, out_file);

	constexpr uint8_t nest_version = NEST_VERSION;
	fwrite(&nest_version, sizeof(uint8_t), 1, out_file);

	fwrite(&file_count, sizeof(uint32_t), 1, out_file);

	size_t current_file = 0;
	auto files = (FILE**) calloc(file_count - 1, sizeof(FILE*));

	uint32_t offset = (sizeof(uint32_t) * 2) + sizeof(uint8_t)        // Header
		+ (((sizeof(uint32_t) * 3) + sizeof(uint16_t)) * file_count); // File descriptions

	size_t project_size = 0;
	char *project_data = project(result.toptab, &project_size);
	write_desc(out_file, project_size, "project", 7, &offset);

	for (int32_t tt = 0; tt < assets.u.tab.size; tt++)
	{
		const char *key = assets.u.tab.key[tt];
		const toml_datum_t value = assets.u.tab.value[tt];

		if (value.type != TOML_ARRAY)
		{
			continue;
		}

		for (int32_t aa = 0; aa < value.u.arr.size; aa++)
		{
			const toml_datum_t item = value.u.arr.elem[aa];
			if (item.type != TOML_STRING)
			{
				continue;
			}

			char *parent = nullptr;
			asprintf(&parent, "%s/assets/%s", path, key);

			char *temp_path = find_file(parent, item.u.str.ptr);
			free(parent);

			if (temp_path == nullptr)
			{
				fprintf(stderr, "file not found: %s/%s\n", key, item.u.str.ptr);
				continue;
			}

			printf("packing: %s\n", temp_path + strlen(path) + 8);

			FILE *temp_file = fopen(temp_path, "rb");
			if (temp_file == nullptr)
			{
				free(temp_path);
				continue;
			}
			free(temp_path);

			files[current_file++] = temp_file;
			const uint32_t temp_size = file_size(temp_file);
			write_desc(out_file, temp_size, item.u.str.ptr, item.u.str.len, &offset);
		}
	}

	fwrite(project_data, sizeof(char), project_size, out_file);
	free(project_data);

	for (size_t i = 0; i < file_count - 1; i++)
	{
		FILE *file = files[i];
		if (file == nullptr)
		{
			continue;
		}

		// File data
		copy(file, out_file);
	}

	for (uint32_t i = 0; i < file_count - 1; i++)
	{
		if (files[i] != nullptr)
		{
			fclose(files[i]);
		}
	}
	free((void*) files);

	toml_free(result);
	fclose(out_file);
	return true;
}

static bool unpack(const char *path)
{
	char *in_path = nullptr;
	asprintf(&in_path, "%s/%s", path, "assets.nest");

	char *out_path = nullptr;
	asprintf(&out_path, "%s/%s", path, "assets.unpacked");

	mkdir(out_path, 0755);

	FILE *in_file = fopen(in_path, "rb");
	if (in_file == nullptr)
	{
		fprintf(stderr, "File not found: %s\n", in_path);
		free(in_path);
		free(out_path);
		return false;
	}

	uint32_t magic = 0;
	fread(&magic, sizeof(uint32_t), 1, in_file);

	if (magic != nest_magic)
	{
		fprintf(stderr, "File is not a valid nest file");
		free(in_path);
		free(out_path);
		return false;
	}

	uint8_t version = 0;
	fread(&version, sizeof(uint8_t), 1, in_file);

	if (version != NEST_VERSION)
	{
		fprintf(stderr, "Unknown nest version: %d", version);
		free(in_path);
		free(out_path);
		return false;
	}

	uint32_t file_count = 0;
	fread(&file_count, sizeof(uint32_t), 1, in_file);

	printf("found %d %s\n", file_count, file_count == 1 ? "file" : "files");

	for (uint32_t i = 0; i < file_count; i++)
	{
		uint32_t hash;
		fread(&hash, sizeof(uint32_t), 1, in_file);
		printf("unpacking: %x\n", hash);

		uint16_t flags;
		fread(&flags, sizeof(uint16_t), 1, in_file);

		uint32_t offset;
		fread(&offset, sizeof(uint32_t), 1, in_file);

		uint32_t size;
		fread(&size, sizeof(uint32_t), 1, in_file);

		char *temp_path = nullptr;
		asprintf(&temp_path, "%s/%x.bin", out_path, hash);

		FILE *temp_file = fopen(temp_path, "wb");
		free(temp_path);

		if (temp_file == nullptr)
		{
			continue;
		}

		const long pos = ftell(in_file);
		fseek(in_file, offset, SEEK_SET);

		copy(in_file, temp_file);

		fclose(temp_file);
		fseek(in_file, pos, SEEK_SET);
	}

	fclose(in_file);
	free(in_path);
	free(out_path);
	return true;
}

int main(const int argc, char **argv)
{
	if (argc != 3)
	{
		printf("usage: %s <pack/unpack> <dir>\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "pack") == 0)
	{
		return (int) pack(argv[2]) ? 0 : 1;
	}

	if (strcmp(argv[1], "unpack") == 0)
	{
		return (int) unpack(argv[2]) ? 0 : 1;
	}

	printf("Unknown command: %s", argv[1]);
	return 1;
}
