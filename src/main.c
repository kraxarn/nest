#include "tomlc17.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool pack(const char *path)
{
	char *in_path = nullptr;
	asprintf(&in_path, "%s/%s", path, "project.toml");

	char *out_path = nullptr;
	asprintf(&out_path, "%s/%s", path, "assets.nest");

	FILE *out_file = fopen(out_path, "wb");
	free(out_path);

	if (out_file == nullptr)
	{
		fprintf(stderr, "Failed to open output path: Error %d\n", errno);
		return false;
	}

	const toml_result_t result = toml_parse_file_ex(in_path);
	free(in_path);

	if (!result.ok)
	{
		fprintf(stderr, "Failed to parse project file: %s\n", result.errmsg);
		fclose(out_file);
		return false;
	}

	const toml_datum_t assets = toml_get(result.toptab, "assets");
	if (assets.type != TOML_TABLE)
	{
		fprintf(stderr, "Invalid project: 'assets' invalid or not found\n");
		fclose(out_file);
		return false;
	}

	uint32_t file_count = 0;
	for (int32_t i = 0; i < assets.u.tab.size; i++)
	{
		const toml_datum_t value = assets.u.tab.value[i];
		if (value.type == TOML_ARRAY)
		{
			file_count += value.u.arr.size;
		}
	}

	printf("found %d %s\n", file_count, file_count == 1 ? "file" : "files");

	// Header

	fwrite(&nest_magic, sizeof(uint32_t), 1, out_file);

	constexpr uint8_t nest_version = NEST_VERSION;
	fwrite(&nest_version, sizeof(uint8_t), 1, out_file);

	fwrite(&file_count, sizeof(uint32_t), 1, out_file);

	uint32_t offset = ftell(out_file);

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

			FILE *temp_file = fopen(temp_path, "rb");
			if (temp_file == nullptr)
			{
				fprintf(stderr, "File not found: %s\n", temp_path);
				free(temp_path);
				continue;
			}

			free(temp_path);
			const uint32_t temp_size = file_size(temp_file);
			fclose(temp_file);

			// File description

			const uint32_t hash = murmur3_32(item.u.str.ptr, item.u.str.len, item.u.str.len);
			fwrite(&hash, sizeof(uint32_t), 1, out_file);

			constexpr uint16_t flags = 0;
			fwrite(&flags, sizeof(uint16_t), 1, out_file);

			fwrite(&offset, sizeof(uint32_t), 1, out_file);
			offset += temp_size;

			fwrite(&temp_size, sizeof(uint32_t), 1, out_file);
		}
	}

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

			FILE *temp_file = fopen(temp_path, "rb");
			if (temp_file == nullptr)
			{
				free(temp_path);
				continue;
			}

			// File data

			constexpr size_t buffer_size = 8192;
			uint8_t buffer[buffer_size];

			size_t read = 0;
			while ((read = fread(buffer, sizeof(uint8_t), buffer_size, temp_file)) > 0)
			{
				fwrite(buffer, sizeof(uint8_t), read, out_file);
			}

			fclose(temp_file);

			printf("packing: %s\n", temp_path + strlen(path) + 8);
			free(temp_path);
		}
	}

	toml_free(result);
	fclose(out_file);
	return true;
}

static bool unpack()
{
	return false;
}

int main(const int argc, char **argv)
{
	if (argc != 3)
	{
		printf("usage: %s <pack/unpack> <project.toml/assets.nest>\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "pack") == 0)
	{
		return (int) pack(argv[2]) ? 0 : 1;
	}

	if (strcmp(argv[1], "unpack") == 0)
	{
		return (int) unpack() ? 0 : 1;
	}

	printf("Unknown command: %s", argv[1]);
	return 1;
}
